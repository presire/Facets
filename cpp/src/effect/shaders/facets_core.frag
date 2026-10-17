#version 140

// facetsフラグメントシェーダ(GLSLコアプロファイル用)
//
// coreプロファイル向けに入力in/出力outとtexture()を使う
// 処理内容と乗算済みアルファの扱いは互換プロファイル版と同一

uniform sampler2D u_texture;
uniform float u_brightness;   // 面の明るさ([Shading]/背景の減光)
uniform float u_opacity;      // 面の不透明度(通常1.0)
uniform float u_aaStrength;   // 縁アンチエイリアスの強さ(0.0で無効)

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    vec4 tex = texture(u_texture, texcoord0);

    // 面の縁までの距離から1px幅で滑らかに落とす
    vec2 d = min(texcoord0, vec2(1.0) - texcoord0);
    float dist = min(d.x, d.y);
    float aa = smoothstep(0.0, max(fwidth(dist), 0.0001) * 1.5, dist);

    // 乗算済みアルファを保ったまま係数kを掛ける
    float k = u_opacity * mix(1.0, aa, u_aaStrength);
    fragColor = vec4(tex.rgb * u_brightness * k, tex.a * k);
}
