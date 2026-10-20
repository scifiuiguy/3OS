// Phase 0.7 Task 7.7 — BASELINE: sky draws over EVERYTHING (ZTest Always + Overlay).
// Direction < 0: inward (MR→VR). Direction >= 0: outward (VR→MR), progress 0=full sky.
// Keep-list exceptions intentionally deferred — do not reintroduce until baseline is confirmed.
Shader "ThreeOS/PalaceSkyOverlay"
{
    Properties
    {
        _FrontTex ("Front", 2D) = "white" {}
        _BackTex ("Back", 2D) = "white" {}
        _LeftTex ("Left", 2D) = "white" {}
        _RightTex ("Right", 2D) = "white" {}
        _UpTex ("Up", 2D) = "white" {}
        _DownTex ("Down", 2D) = "white" {}
        _Tint ("Tint", Color) = (1, 1, 1, 1)
        _Progress ("Progress", Range(0, 1)) = 1
        _Direction ("Direction", Float) = -1
        _Feather ("Feather", Range(0.01, 0.35)) = 0.08
        _CoverBoost ("Cover Boost", Range(0, 1)) = 0
    }

    SubShader
    {
        Tags
        {
            "Queue" = "Overlay"
            "RenderType" = "Transparent"
            "IgnoreProjector" = "True"
            "ForceNoShadowCasting" = "True"
        }

        Pass
        {
            Blend SrcAlpha OneMinusSrcAlpha
            ZWrite Off
            ZTest Always
            Cull Front
            Lighting Off
            Fog { Mode Off }

            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            sampler2D _FrontTex;
            sampler2D _BackTex;
            sampler2D _LeftTex;
            sampler2D _RightTex;
            sampler2D _UpTex;
            sampler2D _DownTex;
            float4 _Tint;
            float _Progress;
            float _Direction;
            float _Feather;
            float _CoverBoost;

            struct appdata
            {
                float4 vertex : POSITION;
            };

            struct v2f
            {
                float4 pos : SV_POSITION;
                float3 viewDir : TEXCOORD0;
                float3 localPos : TEXCOORD1;
            };

            v2f vert(appdata v)
            {
                v2f o;
                o.pos = UnityObjectToClipPos(v.vertex);
                float3 worldPos = mul(unity_ObjectToWorld, v.vertex).xyz;
                o.viewDir = worldPos - _WorldSpaceCameraPos;
                o.localPos = v.vertex.xyz;
                return o;
            }

            float3 SampleSky(float3 d)
            {
                d = normalize(d);
                float3 a = abs(d);
                float2 uv;
                if (a.z >= a.x && a.z >= a.y)
                {
                    uv = float2(d.z > 0 ? d.x : -d.x, d.y) / max(a.z, 1e-5);
                    uv = uv * 0.5 + 0.5;
                    return d.z > 0 ? tex2D(_FrontTex, uv).rgb : tex2D(_BackTex, uv).rgb;
                }
                if (a.x >= a.y)
                {
                    uv = float2(d.x > 0 ? -d.z : d.z, d.y) / max(a.x, 1e-5);
                    uv = uv * 0.5 + 0.5;
                    return d.x > 0 ? tex2D(_RightTex, uv).rgb : tex2D(_LeftTex, uv).rgb;
                }
                uv = float2(d.x, d.y > 0 ? -d.z : d.z) / max(a.y, 1e-5);
                uv = uv * 0.5 + 0.5;
                return d.y > 0 ? tex2D(_UpTex, uv).rgb : tex2D(_DownTex, uv).rgb;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                float3 sky = SampleSky(i.viewDir) * _Tint.rgb;

                float r = length(i.localPos.xz);
                float normR = saturate(r / 0.5);

                float cover;
                if (_Direction >= 0.0)
                {
                    // Outward (VR→MR): progress 0 = full sky, 1 = clear.
                    float threshold = 1.0 - _Progress;
                    cover = 1.0 - smoothstep(threshold - _Feather, threshold + _Feather, normR);
                }
                else
                {
                    // Inward (MR→VR): progress 0 = horizon, 1 = full sky.
                    float inv = 1.0 - _Progress;
                    cover = smoothstep(inv - _Feather, inv + _Feather, normR);
                }
                cover = max(cover, _CoverBoost);
                float alpha = saturate(cover) * _Tint.a;
                return float4(sky, alpha);
            }
            ENDCG
        }
    }

    FallBack Off
}
