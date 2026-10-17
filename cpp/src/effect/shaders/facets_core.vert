#version 140

// facets頂点シェーダ(GLSLコアプロファイル用)
//
// coreプロファイルではattribute/varyingが使えないためin/outを使う
// GLVertexBufferのbindAttributeLocation(position/texcoord)と名前を合わせる
// u_mvpには透視投影または平行投影を掛けた最終行列を渡す

uniform mat4 u_mvp;

in vec3 position;
in vec2 texcoord;

out vec2 texcoord0;

void main()
{
    gl_Position = u_mvp * vec4(position, 1.0);
    texcoord0 = texcoord;
}
