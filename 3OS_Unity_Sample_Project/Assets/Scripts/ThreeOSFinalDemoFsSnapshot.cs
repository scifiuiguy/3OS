using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ThreeOS;
using UnityEngine;
using UnityEngine.Networking;

/// <summary>
/// Offline-serialized Demo1/2 starting layout for Act I reset undo.
/// Loaded once from StreamingAssets — never scanned from disk during the demo.
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

        yield break;
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
    /// Act I reset: put Demo1/2 back to the offline snapshot and clear DemonProject of
    /// pasted snapshot tops. Prefer shell mv/rm — Java delete leaves Documents duplicates.
    /// When a name exists in both DemonProject and a demo home, REPLACE the home copy from
    /// DemonProject (skipping left incomplete/wrong Demo trees after a timed-out restore).
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
        var demo1Set = new HashSet<string>(demo1Tops, StringComparer.OrdinalIgnoreCase);
        var demo2Set = new HashSet<string>(demo2Tops, StringComparer.OrdinalIgnoreCase);
        var owned = new HashSet<string>(demo1Set, StringComparer.OrdinalIgnoreCase);
        foreach (var n in demo2Set)
            owned.Add(n);

        var batchOk = true;
        var moved = 0;
        var scrubbed = 0;
        var replaced = 0;
        ThreeOSAndroidStorage.RunWithJni(() =>
        {
            // 1) Primary: everything still parked under DemonProject → correct home.
            //    Always replace home when DemonProject still has the name (prior restores
            //    left incomplete Demo trees then skipped because LooksPresent(dst)).
            if (LooksPresent(demonRoot))
            {
                var names = ListTopNames(demonRoot);
                foreach (var name in names)
                {
                    if (string.Equals(name, ".keep", StringComparison.OrdinalIgnoreCase) ||
                        name.StartsWith(".", StringComparison.Ordinal))
                        continue;

                    var src = Path.Combine(demonRoot, name);
                    var home = ResolveHome(name, docsDemo1, docsDemo2, demo1Set, demo2Set);
                    if (string.IsNullOrEmpty(home))
                    {
                        if (owned.Contains(name))
                        {
                            // Both homes already have a copy (e.g. simple.glb) — drop Demon leftover.
                            if (ScrubPath(src))
                                scrubbed++;
                            else
                                batchOk = false;
                        }

                        continue;
                    }

                    var dst = Path.Combine(home, name);
                    if (LooksPresent(dst))
                    {
                        // Replace incomplete/wrong home with DemonProject paste source.
                        if (!ScrubPath(dst))
                        {
                            Debug.LogWarning($"[3OS] snapshot replace: could not clear dst {dst}");
                            batchOk = false;
                            continue;
                        }

                        replaced++;
                    }

                    if (RestoreEntry(src, dst))
                    {
                        moved++;
                        if (!LooksPresent(src))
                            scrubbed++;
                        else if (ScrubPath(src))
                            scrubbed++;
                        else
                            batchOk = false;
                    }
                    else
                    {
                        batchOk = false;
                    }
                }
            }

            // 2) Safety: any snapshot top still missing from home — search alt roots.
            foreach (var name in demo1Tops)
            {
                var dst = Path.Combine(docsDemo1 ?? string.Empty, name);
                if (LooksPresent(dst))
                    continue;
                if (RestoreOneFromCandidates(name, docsDemo1, demonRoot, docsDemo2, docsDemo3))
                    moved++;
                else
                    batchOk = false;
            }

            foreach (var name in demo2Tops)
            {
                var dst = Path.Combine(docsDemo2 ?? string.Empty, name);
                if (LooksPresent(dst))
                    continue;
                if (RestoreOneFromCandidates(name, docsDemo2, demonRoot, docsDemo1, docsDemo3))
                    moved++;
                else
                    batchOk = false;
            }

            // 3) Wipe DemonProject tops (keep .keep / dotfiles). Owned or not — leftover
            // paste duplicates block the next cut (ShellMv EPERM onto existing dest) and
            // leave Demo sources behind after copy+delete.
            if (LooksPresent(demonRoot))
            {
                foreach (var name in ListTopNames(demonRoot))
                {
                    if (string.Equals(name, ".keep", StringComparison.OrdinalIgnoreCase) ||
                        name.StartsWith(".", StringComparison.Ordinal))
                        continue;
                    var leftover = Path.Combine(demonRoot, name);
                    if (!LooksPresent(leftover))
                        continue;
                    if (ScrubPath(leftover))
                        scrubbed++;
                    else
                    {
                        Debug.LogWarning($"[3OS] snapshot DemonProject scrub failed {leftover}");
                        batchOk = false;
                    }
                }
            }

            // 4) Drop extras that do not belong in Demo1/2 snapshot tops.
            ScrubUnownedTops(docsDemo1, demo1Set, ref scrubbed, ref batchOk);
            ScrubUnownedTops(docsDemo2, demo2Set, ref scrubbed, ref batchOk);
        });

        Debug.Log(
            $"[3OS] snapshot restore done ok={batchOk} landed={moved} replaced={replaced} " +
            $"scrubbed={scrubbed} d1Tops={demo1Tops.Count} d2Tops={demo2Tops.Count} demon={demonRoot}");
        return batchOk;
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
            if (ThreeOSAndroidStorage.JavaCopyDirectory(src, dst) && LooksPresent(dst))
            {
                Debug.Log($"[3OS] snapshot copy-only OK {src} → {dst}");
                return true;
            }
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
