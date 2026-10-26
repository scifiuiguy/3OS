#if UNITY_EDITOR
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;
using Debug = UnityEngine.Debug;

/// <summary>
/// Batchmode APK builder for Quest sideload (no ADB required).
/// Iterates <c>_NN</c> suffixes from existing files in Builds/Android so each
/// rebuild is a distinct sideload artifact.
/// </summary>
public static class ThreeOSAndroidBuild
{
    private const string ScenePath = "Assets/Scenes/3OS_DemoScene.unity";
    private const string AndroidHandsScenePath = "Assets/Scenes/AndroidHands_ControllerTest.unity";
    private const string FinalDemoScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";
    private const string MicEqTestScenePath = "Assets/Scenes/3OS_MicEq_HudTest.unity";
    private const string OutputDir = "Builds/Android";
    private const string ApkBaseName = "3OS_Sample_Phase05_XR";
    private const string AndroidHandsApkBaseName = "3OS_AndroidHands_ControllerTest";
    private const string FinalDemoApkBaseName = "3OS_Final_Demo";
    private const string MicEqTestApkBaseName = "3OS_MicEq_Test";

    public static void BuildQuestApk()
    {
        BuildApkInternal(ScenePath, ApkBaseName, "3OS Sample");
    }

    /// <summary>
    /// APK for the android01 VR-hands controller test scene.
    /// Batchmode: -executeMethod ThreeOSAndroidBuild.BuildAndroidHandsControllerTestApk
    /// </summary>
    public static void BuildAndroidHandsControllerTestApk()
    {
        BuildApkInternal(AndroidHandsScenePath, AndroidHandsApkBaseName, "3OS Android Hands Test");
    }

    /// <summary>
    /// APK for Phase 0.7 Final Demo scene.
    /// Batchmode: -executeMethod ThreeOSAndroidBuild.BuildFinalDemoApk
    /// </summary>
    public static void BuildFinalDemoApk()
    {
        BuildApkInternal(FinalDemoScenePath, FinalDemoApkBaseName, "3OS Final Demo");
    }

    [MenuItem("3OS/Build/Android Hands Controller Test APK")]
    public static void BuildAndroidHandsControllerTestApkFromMenu()
    {
        BuildApkInternal(AndroidHandsScenePath, AndroidHandsApkBaseName, "3OS Android Hands Test", exitEditor: false);
    }

    [MenuItem("3OS/Build/Final Demo APK")]
    public static void BuildFinalDemoApkFromMenu()
    {
        BuildApkInternal(FinalDemoScenePath, FinalDemoApkBaseName, "3OS Final Demo", exitEditor: false);
    }

    /// <summary>
    /// Minimal mic + HUD EQ diagnostic APK.
    /// Batchmode: -executeMethod ThreeOSAndroidBuild.BuildMicEqTestApk
    /// </summary>
    public static void BuildMicEqTestApk()
    {
        ThreeOSMicEqTestSetup.EnsureScene();
        BuildApkInternal(MicEqTestScenePath, MicEqTestApkBaseName, "3OS Mic EQ Test");
    }

    [MenuItem("3OS/Build/Mic EQ HUD Test APK")]
    public static void BuildMicEqTestApkFromMenu()
    {
        ThreeOSMicEqTestSetup.EnsureScene();
        BuildApkInternal(MicEqTestScenePath, MicEqTestApkBaseName, "3OS Mic EQ Test", exitEditor: false);
    }

    static void BuildApkInternal(string scenePath, string apkBaseName, string productName, bool exitEditor = true)
    {
        if (!File.Exists(scenePath))
        {
            Debug.LogError($"[3OS] Missing scene: {scenePath}");
            if (exitEditor) EditorApplication.Exit(1);
            return;
        }

        if (!EditorUserBuildSettings.SwitchActiveBuildTarget(BuildTargetGroup.Android, BuildTarget.Android))
        {
            Debug.LogError("[3OS] Failed to switch active build target to Android.");
            if (exitEditor) EditorApplication.Exit(1);
            return;
        }

        PlayerSettings.companyName = "scifiuiguy";
        PlayerSettings.productName = productName;
        PlayerSettings.SetApplicationIdentifier(BuildTargetGroup.Android, "com.scifiuiguy.threeos.sample");
        PlayerSettings.Android.minSdkVersion = AndroidSdkVersions.AndroidApiLevel29;
        PlayerSettings.Android.targetSdkVersion = (AndroidSdkVersions)34;
        PlayerSettings.Android.targetArchitectures = AndroidArchitecture.ARM64;
        PlayerSettings.SetScriptingBackend(BuildTargetGroup.Android, ScriptingImplementation.IL2CPP);
        EditorUserBuildSettings.buildAppBundle = false;
        EditorUserBuildSettings.androidBuildSubtarget = MobileTextureSubtarget.ASTC;

        Directory.CreateDirectory(OutputDir);
        // Must never go below an already-installed versionCode (Quest reports downgrade
        // as "package appears to be invalid"). Scan all prior APKs, not only this basename.
        var buildIndex = NextBuildIndex(OutputDir, apkBaseName);
        buildIndex = System.Math.Max(buildIndex, MaxVersionCodeFromAnyApk(OutputDir) + 1);
        buildIndex = System.Math.Max(buildIndex, PlayerSettings.Android.bundleVersionCode + 1);
        PlayerSettings.Android.bundleVersionCode = buildIndex;
        var outputApk = Path.Combine(OutputDir, $"{apkBaseName}_{buildIndex:D2}.apk");

        var options = new BuildPlayerOptions
        {
            scenes = new[] { scenePath },
            locationPathName = outputApk,
            target = BuildTarget.Android,
            options = BuildOptions.CompressWithLz4
        };

        Debug.Log($"[3OS] Building Android APK → {Path.GetFullPath(outputApk)} (build #{buildIndex:D2})");
        var report = BuildPipeline.BuildPlayer(options);
        if (report.summary.result == BuildResult.Succeeded)
        {
            Debug.Log($"[3OS] APK build succeeded: {Path.GetFullPath(outputApk)} ({report.summary.totalSize} bytes)");
            // Final Demo write-through tests need a clean Documents baseline each sideload.
            if (string.Equals(apkBaseName, FinalDemoApkBaseName, System.StringComparison.Ordinal))
                TryResetQuestDemoDocuments();
            if (exitEditor) EditorApplication.Exit(0);
        }
        else
        {
            Debug.LogError($"[3OS] APK build failed: {report.summary.result}");
            if (exitEditor) EditorApplication.Exit(1);
        }
    }

    /// <summary>
    /// Host-side adb wipe+reseed of Documents/3OS_Demo* from the Act I snapshot.
    /// Best-effort: missing adb/device must not fail the APK build.
    /// </summary>
    static void TryResetQuestDemoDocuments()
    {
        try
        {
            // Assets → sample project → repo root (F:/3OS)/tools/...
            var script = Path.GetFullPath(Path.Combine(Application.dataPath, "..", "..", "tools",
                "reset_quest_demo_docs.ps1"));
            if (!File.Exists(script))
            {
                Debug.LogWarning($"[3OS] Quest demo reset script missing: {script}");
                return;
            }

            Debug.Log($"[3OS] Resetting Quest demo Documents via {script}");
            var psi = new ProcessStartInfo
            {
                FileName = "powershell",
                Arguments = $"-NoProfile -ExecutionPolicy Bypass -File \"{script}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };
            using var p = Process.Start(psi);
            if (p == null)
            {
                Debug.LogWarning("[3OS] Quest demo reset: failed to start powershell");
                return;
            }

            var stdout = p.StandardOutput.ReadToEnd();
            var stderr = p.StandardError.ReadToEnd();
            p.WaitForExit(120_000);
            if (!string.IsNullOrWhiteSpace(stdout))
                Debug.Log("[3OS] demo reset:\n" + stdout.Trim());
            if (!string.IsNullOrWhiteSpace(stderr))
                Debug.LogWarning("[3OS] demo reset stderr:\n" + stderr.Trim());
            if (p.ExitCode != 0)
                Debug.LogWarning($"[3OS] Quest demo Documents reset exited {p.ExitCode}");
            else
                Debug.Log("[3OS] Quest demo Documents reset OK");
        }
        catch (System.Exception ex)
        {
            Debug.LogWarning($"[3OS] Quest demo Documents reset skipped: {ex.Message}");
        }
    }

    /// <summary>
    /// Highest existing <c>baseName.apk</c> / <c>baseName_NN.apk</c> index + 1.
    /// Bare <c>baseName.apk</c> counts as 0 so the next file is <c>_01</c>.
    /// </summary>
    internal static int NextBuildIndex(string directory, string baseName)
    {
        var max = 0;
        var hasAny = false;
        if (!Directory.Exists(directory))
        {
            return 1;
        }

        var bare = $"{baseName}.apk";
        var numbered = new Regex("^" + Regex.Escape(baseName) + @"_(\d+)\.apk$",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

        foreach (var path in Directory.GetFiles(directory, "*.apk"))
        {
            var name = Path.GetFileName(path);
            if (string.Equals(name, bare, System.StringComparison.OrdinalIgnoreCase))
            {
                hasAny = true;
                max = System.Math.Max(max, 0);
                continue;
            }

            var m = numbered.Match(name);
            if (!m.Success)
            {
                continue;
            }

            hasAny = true;
            if (int.TryParse(m.Groups[1].Value, out var n))
            {
                max = System.Math.Max(max, n);
            }
        }

        return hasAny ? max + 1 : 1;
    }

    /// <summary>
    /// Highest <c>_NN</c> suffix across every APK in the output dir (any basename).
    /// </summary>
    internal static int MaxVersionCodeFromAnyApk(string directory)
    {
        var max = 0;
        if (!Directory.Exists(directory))
        {
            return 0;
        }

        var numbered = new Regex(@"_(\d+)\.apk$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        foreach (var path in Directory.GetFiles(directory, "*.apk"))
        {
            var m = numbered.Match(Path.GetFileName(path));
            if (m.Success && int.TryParse(m.Groups[1].Value, out var n))
            {
                max = System.Math.Max(max, n);
            }
        }

        return max;
    }
}
#endif

