// facets頂点シェーダ(互換プロファイル用)
//
// 独自のMVPをそのまま適用し、テクスチャ座標を面のuvとして渡すだけの最小構成
// 属性名position/texcoordはGLShaderのbindAttributeLocationと対応している
// 透視投影と背景用の平行投影は呼び出し側で1本のu_mvpにまとめて渡す

uniform mat4 u_mvp;

attribute vec3 position;
attribute vec2 texcoord;

varying vec2 texcoord0;

void main()
{
    gl_Position = u_mvp * vec4(position, 1.0);
    texcoord0 = texcoord;
}
