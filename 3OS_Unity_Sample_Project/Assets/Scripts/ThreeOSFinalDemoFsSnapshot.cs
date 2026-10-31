using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ThreeOS;
using UnityEngine;
using UnityEngine.Networking;

/// <summary>
/// Offline Act I baselines: Demo1/2 tree from <see cref="Snapshot.demo1"/> /
/// <see cref="Snapshot.demo2"/>; Demo3 paste box from a separate baseline (empty
/// DemonProject + markers). Loaded from StreamingAssets — not inferred from session cut/paste.
/// </summary>
public static class ThreeOSFinalDemoFsSnapshot
{
    const string StreamingRelPath = "3OS/final_demo_fs_snapshot.json";

    [Serializable]
    public class Snapshot
    {
        public int version = 1;
        public string demo1_folder = "3OS_Demo";
        public string demo2_folder = "3OS_Demo2";
        public string demo3_folder = "3OS_Demo3";
        public string demon_project = "DemonProject";
        public string[] demo1 = Array.Empty<string>();
        public string[] demo2 = Array.Empty<string>();
    }

    static Snapshot _cached;
    static bool _loadAttempted;
    static byte[] _seedSimpleGlb;
    static byte[] _seedDemonA;
    static bool _seedLoadAttempted;

    const string SeedSimpleRel = "3OS/seed/simple.glb";
    const string SeedDemonRel = "3OS/seed/DemonA.blend";

    public static Snapshot Cached => _cached;

    public static IEnumerator EnsureLoaded()
    {
        if (_cached != null || _loadAttempted)
            yield break;
        _loadAttempted = true;

        var path = Path.Combine(Application.streamingAssetsPath, StreamingRelPath);
#if UNITY_ANDROID && !UNITY_EDITOR
        using (var req = UnityWebRequest.Get(path))
        {
            yield return req.SendWebRequest();
            if (req.result != UnityWebRequest.Result.Success ||
                string.IsNullOrEmpty(req.downloadHandler?.text))
            {
                Debug.LogWarning($"[3OS] FS snapshot load failed: {req.error} url={path}");
            }
            else
            {
                try
                {
                    _cached = JsonUtility.FromJson<Snapshot>(req.downloadHandler.text);
                }
                catch (Exception ex)
                {
                    Debug.LogWarning($"[3OS] FS snapshot parse failed: {ex.Message}");
                }
            }
        }
#else
        try
        {
            if (File.Exists(path))
                _cached = JsonUtility.FromJson<Snapshot>(File.ReadAllText(path));
            else
                Debug.LogWarning($"[3OS] FS snapshot missing: {path}");
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] FS snapshot read failed: {ex.Message}");
        }
#endif
        if (_cached == null ||
            ((_cached.demo1 == null || _cached.demo1.Length == 0) &&
             (_cached.demo2 == null || _cached.demo2.Length == 0)))
        {
            Debug.LogWarning("[3OS] FS snapshot empty/unparsed — using embedded Final Demo defaults");
            _cached = EmbeddedDefault();
        }
        else
        {
            Debug.Log(
                $"[3OS] FS snapshot loaded demo1={_cached.demo1?.Length ?? 0} demo2={_cached.demo2?.Length ?? 0}");
        }

        if (!_seedLoadAttempted)
        {
            _seedLoadAttempted = true;
            yield return LoadSeedBytesRoutine(SeedSimpleRel, b => _seedSimpleGlb = b);
            yield return LoadSeedBytesRoutine(SeedDemonRel, b => _seedDemonA = b);
        }

        yield break;
    }

    static IEnumerator LoadSeedBytesRoutine(string streamingRel, Action<byte[]> assign)
    {
        var url = Path.Combine(Application.streamingAssetsPath, streamingRel.Replace('\\', '/'));
#if UNITY_ANDROID && !UNITY_EDITOR
        using (var req = UnityWebRequest.Get(url))
        {
            yield return req.SendWebRequest();
            if (req.result == UnityWebRequest.Result.Success &&
                req.downloadHandler?.data != null && req.downloadHandler.data.Length > 0)
                assign(req.downloadHandler.data);
            else
                Debug.LogWarning($"[3OS] FS seed load failed: {req.error} url={url}");
        }
#else
        try
        {
            if (File.Exists(url))
                assign(File.ReadAllBytes(url));
            else
                Debug.LogWarning($"[3OS] FS seed missing: {url}");
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] FS seed read failed: {ex.Message}");
        }

        yield break;
#endif
    }

    static Snapshot EmbeddedDefault() => new Snapshot
    {
        version = 1,
        demo1_folder = "3OS_Demo",
        demo2_folder = "3OS_Demo2",
        demo3_folder = "3OS_Demo3",
        demon_project = "DemonProject",
        demo1 = new[]
        {
            "DemonA.blend", "simple.glb", "Test1", "Test11", "Test12", "Test13", "Test19",
            "Test19/Test2", "Test20", "Test21", "Test3", "Test3/simple.glb", "Test5",
            "Test5/simple.glb", "Test6", "Test6/DemonA.blend", "Test7", "Test9", "Test9/Test8"
        },
        demo2 = new[]
        {
            "simple.glb", "TestA", "TestB", "TestC", "TestC/simple.glb", "TestD", "TestD/simple.glb",
            "TestE", "TestF", "TestG", "TestH", "TestI", "TestJ", "TestK", "TestL", "TestM", "TestN",
            "TestO", "TestP", "TestQ", "TestR", "TestS", "TestT", "TestU", "TestV", "TestW", "TestX",
            "TestY", "TestZ"
        }
    };

    /// <summary>
    /// Act I reset: replay offline baselines — wipe Documents Demo1/2/3 and materialize from
    /// StreamingAssets snapshot (same contract as <c>tools/reset_quest_demo_docs.ps1</c>).
    /// Does not move files from DemonProject or undo in-session cut/paste.
    /// </summary>
    public static bool RestoreDemoRootsFromSnapshot(Snapshot snap, string docsDemo1, string docsDemo2,
        string docsDemo3)
    {
        if (snap == null)
            return false;

        var demon = string.IsNullOrEmpty(snap.demon_project) ? "DemonProject" : snap.demon_project;
        var demonRoot = Path.Combine(docsDemo3 ?? string.Empty, demon);

        var demo1Tops = TopLevelNames(snap.demo1);
        var demo2Tops = TopLevelNames(snap.demo2);

        var batchOk = true;
        var scrubbed = 0;
        var seeded = 0;
        ThreeOSAndroidStorage.RunWithJni(() =>
        {
            // 1) Wipe current Documents trees (session state is discarded).
            WipeRootContents(docsDemo1, ref scrubbed, ref batchOk);
            WipeRootContents(docsDemo2, ref scrubbed, ref batchOk);
            WipeRootContents(docsDemo3, ref scrubbed, ref batchOk);

            // 2) Demo1 / Demo2 — offline snapshot only.
            if (snap.demo1 != null)
            {
                foreach (var raw in snap.demo1)
                {
                    if (MaterializeSnapshotRel(docsDemo1, raw))
                        seeded++;
                    else
                        batchOk = false;
                }
            }

            if (snap.demo2 != null)
            {
                foreach (var raw in snap.demo2)
                {
                    if (MaterializeSnapshotRel(docsDemo2, raw))
                        seeded++;
                    else
                        batchOk = false;
                }
            }

            // 3) Demo3 — separate baseline (empty paste box + Mind Palace marker).
            if (!RestoreDemo3Baseline(docsDemo3, demonRoot))
                batchOk = false;
        });

        Debug.Log(
            $"[3OS] snapshot baseline replay ok={batchOk} seeded={seeded} scrubbed={scrubbed} " +
            $"d1Tops={demo1Tops.Count} d2Tops={demo2Tops.Count} d3={docsDemo3} demon={demonRoot}");
        return batchOk;
    }

    /// <summary>Empty DemonProject + <c>.3os_mind_palace</c> on Demo3 root (host reset script parity).</summary>
    static bool RestoreDemo3Baseline(string docsDemo3, string demonRoot)
    {
        if (string.IsNullOrEmpty(docsDemo3))
            return false;

        EnsureDirectory(docsDemo3);
        EnsureDirectory(demonRoot);

        var ok = true;
        var keepPath = Path.Combine(demonRoot, ".keep");
        if (!WriteAllBytes(keepPath, System.Text.Encoding.UTF8.GetBytes("keep")))
        {
            Debug.LogWarning($"[3OS] snapshot Demo3 baseline: .keep failed {keepPath}");
            ok = false;
        }

        var mpPath = Path.Combine(docsDemo3, ".3os_mind_palace");
        if (!WriteAllBytes(mpPath, System.Text.Encoding.UTF8.GetBytes("mindpalace")))
        {
            Debug.LogWarning($"[3OS] snapshot Demo3 baseline: mind palace marker failed {mpPath}");
            ok = false;
        }

        return ok;
    }

    static void WipeRootContents(string root, ref int scrubbed, ref bool batchOk)
    {
        if (string.IsNullOrEmpty(root))
            return;
        if (!LooksPresent(root))
        {
            EnsureDirectory(root);
            return;
        }

        foreach (var name in ListTopNames(root))
        {
            var path = Path.Combine(root, name);
            if (ScrubPath(path))
                scrubbed++;
            else
            {
                Debug.LogWarning($"[3OS] snapshot wipe failed {path}");
                batchOk = false;
            }
        }
    }

    /// <summary>
    /// Ensure one snapshot relative path exists under a demo home (StreamingAssets seed or empty stub).
    /// </summary>
    static bool MaterializeSnapshotRel(string homeRoot, string rawRel)
    {
        if (string.IsNullOrEmpty(homeRoot))
            return false;
        var rel = (rawRel ?? string.Empty).Replace('\\', '/').Trim().Trim('/');
        if (string.IsNullOrEmpty(rel))
            return true;

        var dest = Path.Combine(homeRoot, rel);
        if (LooksPresent(dest))
            return true;

        try
        {
            var parent = Path.GetDirectoryName(dest);
            if (!string.IsNullOrEmpty(parent))
                EnsureDirectory(parent);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot seed mkdir parent {dest}: {ex.Message}");
        }

        var leaf = Path.GetFileName(rel);
        var isFile = rel.IndexOf('.') >= 0;
        if (!isFile)
        {
            EnsureDirectory(dest);
            return LooksPresent(dest);
        }

        byte[] payload = null;
        if (string.Equals(leaf, "simple.glb", StringComparison.OrdinalIgnoreCase))
            payload = _seedSimpleGlb;
        else if (string.Equals(leaf, "DemonA.blend", StringComparison.OrdinalIgnoreCase))
            payload = _seedDemonA;

        if (payload != null && payload.Length > 0)
        {
            if (WriteAllBytes(dest, payload))
            {
                Debug.Log($"[3OS] snapshot seed wrote {dest} ({payload.Length} bytes)");
                return true;
            }

            return false;
        }

        // Folder marker files (Test*.txt etc.) — empty stub matches host reset_quest_demo_docs.ps1.
        if (WriteAllBytes(dest, Array.Empty<byte>()))
            return true;

        Debug.LogWarning($"[3OS] snapshot seed failed {dest}");
        return false;
    }

    static void EnsureDirectory(string path)
    {
        if (string.IsNullOrEmpty(path) || LooksPresent(path))
            return;
        try
        {
            Directory.CreateDirectory(path);
        }
        catch { /* Java mkdirs below */ }

        if (LooksPresent(path))
            return;
#if UNITY_ANDROID && !UNITY_EDITOR
        try
        {
            using var f = new AndroidJavaObject("java.io.File", path);
            f.Call<bool>("mkdirs");
        }
        catch { /* best-effort */ }
#endif
    }

    static bool WriteAllBytes(string path, byte[] data)
    {
        if (string.IsNullOrEmpty(path))
            return false;
        data ??= Array.Empty<byte>();
        try
        {
            var parent = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(parent))
                EnsureDirectory(parent);
        }
        catch { /* best-effort */ }

#if UNITY_ANDROID && !UNITY_EDITOR
        try
        {
            using var f = new AndroidJavaObject("java.io.File", path);
            using var parent = f.Call<AndroidJavaObject>("getParentFile");
            parent?.Call<bool>("mkdirs");
            using var fos = new AndroidJavaObject("java.io.FileOutputStream", f);
            if (data.Length == 0)
            {
                fos.Call("close");
                return LooksPresent(path);
            }

            var sbytes = new sbyte[data.Length];
            for (var i = 0; i < data.Length; i++)
                sbytes[i] = unchecked((sbyte)data[i]);
            fos.Call("write", sbytes);
            fos.Call("close");
            return LooksPresent(path);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot seed java write {path}: {ex.Message}");
        }
#endif
        try
        {
            File.WriteAllBytes(path, data);
            return LooksPresent(path);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot seed write {path}: {ex.Message}");
            return false;
        }
    }

    static void ScrubUnownedTops(string root, HashSet<string> owned, ref int scrubbed, ref bool batchOk)
    {
        if (string.IsNullOrEmpty(root) || !LooksPresent(root))
            return;
        foreach (var name in ListTopNames(root))
        {
            if (owned.Contains(name))
                continue;
            if (name.StartsWith(".", StringComparison.Ordinal))
                continue;
            var path = Path.Combine(root, name);
            if (ScrubPath(path))
                scrubbed++;
            else
            {
                Debug.LogWarning($"[3OS] snapshot unowned scrub failed {path}");
                batchOk = false;
            }
        }
    }

    static List<string> ListTopNames(string dir)
    {
        var names = new List<string>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        try
        {
            foreach (var n in ThreeOSAndroidStorage.ListDirectoryNames(dir))
            {
                if (seen.Add(n))
                    names.Add(n);
            }

            foreach (var n in ThreeOSAndroidStorage.ListFileNames(dir))
            {
                if (seen.Add(n))
                    names.Add(n);
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] list tops {dir}: {ex.Message}");
        }

        // System.IO fallback when Java list is empty on a present dir.
        if (names.Count == 0 && Directory.Exists(dir))
        {
            try
            {
                foreach (var p in Directory.GetFileSystemEntries(dir))
                {
                    var n = Path.GetFileName(p);
                    if (!string.IsNullOrEmpty(n) && seen.Add(n))
                        names.Add(n);
                }
            }
            catch { /* best-effort */ }
        }

        return names;
    }

    static string ResolveHome(string name, string docsDemo1, string docsDemo2,
        HashSet<string> demo1Set, HashSet<string> demo2Set)
    {
        var in1 = demo1Set.Contains(name);
        var in2 = demo2Set.Contains(name);
        if (in1 && in2)
        {
            // simple.glb lives in both snapshots — fill whichever home is empty first.
            var d1 = Path.Combine(docsDemo1 ?? string.Empty, name);
            var d2 = Path.Combine(docsDemo2 ?? string.Empty, name);
            if (!LooksPresent(d1))
                return docsDemo1;
            if (!LooksPresent(d2))
                return docsDemo2;
            // Both already have a copy — scrub DemonProject only (home = null via empty).
            return null;
        }

        if (in1)
            return docsDemo1;
        if (in2)
            return docsDemo2;
        return null;
    }

    static List<string> TopLevelNames(string[] rels)
    {
        var list = new List<string>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (rels == null)
            return list;
        foreach (var raw in rels)
        {
            var rel = (raw ?? string.Empty).Replace('\\', '/').Trim().Trim('/');
            if (string.IsNullOrEmpty(rel))
                continue;
            var slash = rel.IndexOf('/');
            var top = slash < 0 ? rel : rel.Substring(0, slash);
            if (seen.Add(top))
                list.Add(top);
        }

        return list;
    }

    static bool RestoreOneFromCandidates(string rel, string homeRoot, string demonRoot,
        string otherDemo, string demo3Root)
    {
        rel = (rel ?? string.Empty).Replace('\\', '/').Trim().Trim('/');
        if (string.IsNullOrEmpty(rel) || string.IsNullOrEmpty(homeRoot) || rel.IndexOf('/') >= 0)
            return LooksPresent(Path.Combine(homeRoot ?? string.Empty, rel));

        var dest = Path.Combine(homeRoot, rel);
        var candidates = new List<string>
        {
            Path.Combine(demonRoot ?? string.Empty, rel),
        };
        if (!string.IsNullOrEmpty(demo3Root))
            candidates.Add(Path.Combine(demo3Root, rel));

        var homeLeaf = Path.GetFileName(homeRoot.TrimEnd('/', '\\'));
        if (!string.IsNullOrEmpty(homeLeaf) && !string.IsNullOrEmpty(demonRoot))
            candidates.Add(Path.Combine(demonRoot, homeLeaf, rel));

        // Prefer move/scrub from DemonProject / Demo3. Only copy from the other Demo root
        // (shared simple.glb) — never steal the sibling home's only copy.
        if (!string.IsNullOrEmpty(otherDemo))
            candidates.Add(Path.Combine(otherDemo, rel));

        foreach (var c in candidates)
        {
            if (string.IsNullOrEmpty(c) || !LooksPresent(c))
                continue;
            if (string.Equals(c, dest, StringComparison.OrdinalIgnoreCase))
                continue;

            var fromOtherDemo = !string.IsNullOrEmpty(otherDemo) &&
                c.StartsWith(otherDemo.TrimEnd('/', '\\') + Path.DirectorySeparatorChar,
                    StringComparison.OrdinalIgnoreCase);
            if (!fromOtherDemo && !string.IsNullOrEmpty(otherDemo))
            {
                var alt = otherDemo.TrimEnd('/', '\\') + "/";
                fromOtherDemo = c.StartsWith(alt, StringComparison.OrdinalIgnoreCase);
            }

            if (fromOtherDemo)
                return CopyOnly(c, dest);
            return RestoreEntry(c, dest);
        }

        return LooksPresent(dest);
    }

    static bool CopyOnly(string src, string dst)
    {
        if (string.IsNullOrEmpty(src) || string.IsNullOrEmpty(dst) || !LooksPresent(src))
            return LooksPresent(dst);
        if (LooksPresent(dst))
            return true;

        try
        {
            var parent = Path.GetDirectoryName(dst);
            if (!string.IsNullOrEmpty(parent))
            {
                try { Directory.CreateDirectory(parent); }
                catch { /* Java mkdirs */ }
            }
        }
        catch { /* best-effort */ }

#if UNITY_ANDROID && !UNITY_EDITOR
        var treatedAsDir = Directory.Exists(src);
        if (!treatedAsDir)
        {
            try
            {
                using var f = new AndroidJavaObject("java.io.File", src);
                treatedAsDir = f.Call<bool>("isDirectory");
            }
            catch { /* assume file */ }
        }

        if (treatedAsDir)
        {
            if (ThreeOSAndroidStorage.ShellCpTree(src, dst) && LooksPresent(dst))
            {
                Debug.Log($"[3OS] snapshot copy-only OK {src} → {dst}");
                return true;
            }

            if (ThreeOSAndroidStorage.JavaCopyDirectory(src, dst) && LooksPresent(dst))
            {
                Debug.Log($"[3OS] snapshot copy-only OK {src} → {dst}");
                return true;
            }
        }
        else
        {
            if (ThreeOSAndroidStorage.ShellCpTree(src, dst) && LooksPresent(dst))
            {
                Debug.Log($"[3OS] snapshot copy-only OK {src} → {dst}");
                return true;
            }

            try
            {
                using var paths = new AndroidJavaClass("java.nio.file.Paths");
                using var filesCls = new AndroidJavaClass("java.nio.file.Files");
                var emptyMore = new string[0];
                using var sp = paths.CallStatic<AndroidJavaObject>("get", src, emptyMore);
                using var dp = paths.CallStatic<AndroidJavaObject>("get", dst, emptyMore);
                using var optionCls = new AndroidJavaClass("java.nio.file.StandardCopyOption");
                using var replace = optionCls.GetStatic<AndroidJavaObject>("REPLACE_EXISTING");
                var opts = new AndroidJavaObject[] { replace };
                filesCls.CallStatic<AndroidJavaObject>("copy", sp, dp, opts);
                if (LooksPresent(dst))
                {
                    Debug.Log($"[3OS] snapshot copy-only OK {src} → {dst}");
                    return true;
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[3OS] snapshot copy-only {src}: {ex.Message}");
            }
        }
#else
        try
        {
            if (Directory.Exists(src))
            {
                CopyDirectoryManaged(src, dst);
                return LooksPresent(dst);
            }

            File.Copy(src, dst, true);
            return LooksPresent(dst);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot copy-only managed {src}: {ex.Message}");
        }
#endif
        return LooksPresent(dst);
    }

#if !UNITY_ANDROID || UNITY_EDITOR
    static void CopyDirectoryManaged(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var file in Directory.GetFiles(src))
            File.Copy(file, Path.Combine(dst, Path.GetFileName(file)), true);
        foreach (var dir in Directory.GetDirectories(src))
            CopyDirectoryManaged(dir, Path.Combine(dst, Path.GetFileName(dir)));
    }
#endif

    /// <summary>
    /// Move/copy <paramref name="src"/> onto <paramref name="dst"/> (dst must be absent),
    /// then scrub src if it remains (NIO/MTP ghost).
    /// </summary>
    static bool RestoreEntry(string src, string dst)
    {
        if (string.IsNullOrEmpty(src) || string.IsNullOrEmpty(dst) || !LooksPresent(src))
            return LooksPresent(dst);

        if (string.Equals(src, dst, StringComparison.OrdinalIgnoreCase))
            return true;

        if (LooksPresent(dst))
        {
            if (!ScrubPath(dst))
            {
                Debug.LogWarning($"[3OS] snapshot restore: dst occupied {dst}");
                return false;
            }
        }

        try
        {
            var parent = Path.GetDirectoryName(dst);
            if (!string.IsNullOrEmpty(parent))
            {
                try { Directory.CreateDirectory(parent); }
                catch { /* Java mkdirs below */ }
            }
        }
        catch { /* best-effort */ }

        if (!TryMoveOrCopy(src, dst))
        {
            Debug.LogWarning($"[3OS] snapshot restore incomplete {src} → {dst}");
            return false;
        }

        var destOk = LooksPresent(dst);
        Debug.Log($"[3OS] snapshot restore OK {src} → {dst} destOk={destOk}");

        if (LooksPresent(src) && !ScrubPath(src))
            Debug.LogWarning($"[3OS] snapshot scrub failed (src remains) {src}");

        return destOk;
    }

    static bool TryMoveOrCopy(string src, string dst)
    {
        // Shell mv first — same path paste uses successfully on Quest Documents.
#if UNITY_ANDROID && !UNITY_EDITOR
        if (ThreeOSAndroidStorage.ShellMv(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.JavaNioMove(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.OsRename(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.JavaRename(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.ShellCpTree(src, dst) && LooksPresent(dst))
            return true;
#endif

        // Managed move.
        try
        {
            if (Directory.Exists(src))
            {
                if (Directory.Exists(dst))
                    ScrubPath(dst);
                Directory.Move(src, dst);
                if (LooksPresent(dst))
                    return true;
            }
            else if (File.Exists(src) || ThreeOSAndroidStorage.Exists(src))
            {
                if (File.Exists(dst) || ThreeOSAndroidStorage.Exists(dst))
                    ScrubPath(dst);
                if (File.Exists(src))
                {
                    File.Move(src, dst);
                    if (LooksPresent(dst))
                        return true;
                }
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot managed move {src} → {dst}: {ex.Message}");
        }

#if UNITY_ANDROID && !UNITY_EDITOR
        // Folder/file copy (paste's decisive path when rename fails).
        if (Directory.Exists(src) || ThreeOSAndroidStorage.Exists(src))
        {
            var treatedAsDir = Directory.Exists(src);
            if (!treatedAsDir)
            {
                try
                {
                    using var f = new AndroidJavaObject("java.io.File", src);
                    treatedAsDir = f.Call<bool>("isDirectory");
                }
                catch { /* assume file */ }
            }

            if (treatedAsDir)
            {
                if (ThreeOSAndroidStorage.ShellCpTree(src, dst) && LooksPresent(dst))
                    return true;
                if (ThreeOSAndroidStorage.JavaCopyDirectory(src, dst) && LooksPresent(dst))
                    return true;
            }
            else
            {
                try
                {
                    using var paths = new AndroidJavaClass("java.nio.file.Paths");
                    using var filesCls = new AndroidJavaClass("java.nio.file.Files");
                    var emptyMore = new string[0];
                    using var sp = paths.CallStatic<AndroidJavaObject>("get", src, emptyMore);
                    using var dp = paths.CallStatic<AndroidJavaObject>("get", dst, emptyMore);
                    using var optionCls = new AndroidJavaClass("java.nio.file.StandardCopyOption");
                    using var replace = optionCls.GetStatic<AndroidJavaObject>("REPLACE_EXISTING");
                    var opts = new AndroidJavaObject[] { replace };
                    filesCls.CallStatic<AndroidJavaObject>("copy", sp, dp, opts);
                    if (LooksPresent(dst))
                        return true;
                }
                catch (Exception ex)
                {
                    Debug.LogWarning($"[3OS] snapshot file copy {src}: {ex.Message}");
                }
            }
        }
#endif
        return LooksPresent(dst);
    }

    static bool ScrubPath(string path)
    {
        if (string.IsNullOrEmpty(path) || !LooksPresent(path))
            return true;

#if UNITY_ANDROID && !UNITY_EDITOR
        if (ThreeOSAndroidStorage.ShellRm(path) && !LooksPresent(path))
            return true;
#endif
        ThreeOSAndroidStorage.JavaDeleteRecursive(path);
        if (!LooksPresent(path))
            return true;
        try
        {
            if (Directory.Exists(path))
                Directory.Delete(path, true);
            else if (File.Exists(path))
                File.Delete(path);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot scrub managed {path}: {ex.Message}");
        }

        return !LooksPresent(path);
    }

    static bool LooksPresent(string path) =>
        !string.IsNullOrEmpty(path) &&
        (Directory.Exists(path) || File.Exists(path) || ThreeOSAndroidStorage.Exists(path));
}
