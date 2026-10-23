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
    /// Move Demo1/2 entries back to their snapshot homes (from DemonProject or misplaced roots).
    /// Safe on ThreadPool — Quest Documents uses Java NIO when System.IO Move fails.
    /// </summary>
    public static bool RestoreDemoRootsFromSnapshot(Snapshot snap, string docsDemo1, string docsDemo2,
        string docsDemo3)
    {
        if (snap == null)
            return false;

        var ok = true;
        var demon = string.IsNullOrEmpty(snap.demon_project) ? "DemonProject" : snap.demon_project;
        var demonRoot = Path.Combine(docsDemo3 ?? string.Empty, demon);

        // Attach JNI once for the whole batch (folder NIO moves need it on Android workers).
        var batchOk = true;
        ThreeOSAndroidStorage.RunWithJni(() =>
        {
            if (snap.demo1 != null)
            {
                foreach (var rel in snap.demo1)
                {
                    if (!RestoreOne(rel, docsDemo1, demonRoot, docsDemo2, docsDemo3))
                        batchOk = false;
                }
            }

            if (snap.demo2 != null)
            {
                foreach (var rel in snap.demo2)
                {
                    if (!RestoreOne(rel, docsDemo2, demonRoot, docsDemo1, docsDemo3))
                        batchOk = false;
                }
            }
        });

        return batchOk && ok;
    }

    static bool RestoreOne(string rel, string homeRoot, string demonRoot, string otherDemo,
        string demo3Root)
    {
        rel = (rel ?? string.Empty).Replace('\\', '/').Trim().Trim('/');
        if (string.IsNullOrEmpty(rel) || string.IsNullOrEmpty(homeRoot))
            return false;

        var dest = Path.Combine(homeRoot, rel.Replace('/', Path.DirectorySeparatorChar));

        var candidates = new List<string>(8)
        {
            Path.Combine(demonRoot ?? string.Empty, rel.Replace('/', Path.DirectorySeparatorChar)),
            Path.Combine(otherDemo ?? string.Empty, rel.Replace('/', Path.DirectorySeparatorChar)),
            Path.Combine(demo3Root ?? string.Empty, rel.Replace('/', Path.DirectorySeparatorChar)),
        };

        // Nested under DemonProject with home leaf prefix (legacy bad pastes).
        var homeLeaf = Path.GetFileName(homeRoot.TrimEnd('/', '\\'));
        if (!string.IsNullOrEmpty(homeLeaf) && !string.IsNullOrEmpty(demonRoot))
        {
            candidates.Add(Path.Combine(demonRoot, homeLeaf,
                rel.Replace('/', Path.DirectorySeparatorChar)));
        }

        // Also search sibling leaf names under Documents (Demo vs 3OS_Demo renames).
        if (!string.IsNullOrEmpty(homeLeaf))
        {
            foreach (var altLeaf in new[] { "3OS_Demo", "Demo", "3OS_Demo1" })
            {
                if (string.Equals(altLeaf, homeLeaf, StringComparison.OrdinalIgnoreCase))
                    continue;
                var altRoot = Path.Combine(Path.GetDirectoryName(homeRoot.TrimEnd('/', '\\')) ?? string.Empty,
                    altLeaf);
                candidates.Add(Path.Combine(altRoot, rel.Replace('/', Path.DirectorySeparatorChar)));
            }
        }

        string found = null;
        foreach (var c in candidates)
        {
            if (string.IsNullOrEmpty(c) || !LooksPresent(c))
                continue;
            // Prefer a source that is NOT already the destination.
            if (string.Equals(c, dest, StringComparison.OrdinalIgnoreCase))
                continue;
            found = c;
            break;
        }

        // Already home and nowhere else — done.
        if (found == null)
            return LooksPresent(dest);

        try
        {
            var parent = Path.GetDirectoryName(dest);
            if (!string.IsNullOrEmpty(parent))
            {
                try
                {
                    Directory.CreateDirectory(parent);
                }
                catch
                {
                    // Java mkdirs below may still succeed on Documents.
                }
            }

            // Quest Documents: System.IO Move often fails (same as paste). Try NIO / Os / shell.
            if (TryMovePath(found, dest))
            {
                Debug.Log($"[3OS] snapshot restore OK {found} → {dest}");
                return LooksPresent(dest);
            }

            Debug.LogWarning($"[3OS] snapshot restore incomplete {found} → {dest}");
            return false;
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot restore failed {found} → {dest}: {ex.Message}");
            return false;
        }
    }

    static bool TryMovePath(string src, string dst)
    {
        if (string.IsNullOrEmpty(src) || string.IsNullOrEmpty(dst))
            return false;

        // Managed first (works for persistent / some Documents files).
        try
        {
            if (Directory.Exists(src))
            {
                if (Directory.Exists(dst))
                    Directory.Delete(dst, true);
                Directory.Move(src, dst);
                if (LooksPresent(dst))
                    return true;
            }
            else if (File.Exists(src))
            {
                if (File.Exists(dst))
                    File.Delete(dst);
                File.Move(src, dst);
                if (LooksPresent(dst))
                    return true;
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS] snapshot managed move {src} → {dst}: {ex.Message}");
        }

#if UNITY_ANDROID && !UNITY_EDITOR
        // Same ladder as paste folder cut — Documents needs Java NIO.
        if (ThreeOSAndroidStorage.JavaNioMove(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.OsRename(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.ShellMv(src, dst) && LooksPresent(dst))
            return true;
        if (ThreeOSAndroidStorage.JavaRename(src, dst) && LooksPresent(dst))
            return true;
#endif
        return LooksPresent(dst) && !LooksPresent(src);
    }

    static bool LooksPresent(string path) =>
        !string.IsNullOrEmpty(path) &&
        (Directory.Exists(path) || File.Exists(path) || ThreeOSAndroidStorage.Exists(path));
}
