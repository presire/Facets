/*
    facets — バーチャルデスクトップ切替時の 3D アニメーション (描画部)

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "facets.h"

#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QVector2D>
#include <QVector3D>

#include <core/output.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <opengl/glutils.h>

#include <algorithm>
#include <cmath>
#include <array>
#include <span>
#include <vector>

namespace KWin
{

namespace
{

/** @brief ミップマップを含むテクスチャのレベル数 */
constexpr int kMipLevels = 4;

/** @brief 透視投影の垂直画角(度) */
constexpr double kFovDegrees = 45.0;

/** @brief 円周率 */
constexpr double kPi = 3.14159265358979323846;

/** @brief 頂点シェーダのリソースパス */
constexpr const char *kVertexShader = ":/effects/facets/shaders/facets.vert";

/** @brief フラグメントシェーダのリソースパス */
constexpr const char *kFragmentShader = ":/effects/facets/shaders/facets.frag";

/**
 * @brief 度をラジアンへ変換する
 * @param degrees 度
 * @return ラジアン
 */
double toRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

/**
 * @brief 論理座標でのビューの寸法を返す
 * @param viewport ビューポート
 * @return 幅と高さ
 */
QSizeF logicalSize(const RenderViewport &viewport)
{
    return viewport.renderRect().size();
}

/**
 * @brief 進行軸に沿った立方体の半径を返す
 * @param size 画面の論理サイズ
 * @param vertical 縦方向の切替か
 * @return 半径(進行方向の寸法の半分)
 */
double cubeRadius(const QSizeF &size, bool vertical)
{
    return (vertical ? size.height() : size.width()) * 0.5;
}

} // 無名名前空間の終わり

/**
 * @brief 画面描画のエントリポイント
 * @param renderTarget 描画先のレンダーターゲット
 * @param viewport 描画に使うビューポート
 * @param mask 描画対象のマスク
 * @param region 描画対象の領域
 * @param screen 対象の出力
 *
 * 再入やリソース未用意のときは親実装へ素通しする
 * 撮影内容が有効な間は再利用し、切替開始時や対象ウィンドウの構成変更時に撮り直す
 */
void FacetsEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const DeviceRegion &region, ScreenOutput *screen)
{
    // 非動作中とキャプチャ中の再入は素通しにする
    if (!m_active || m_pass != Pass::None || !screen) {
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }

    m_curRenderTarget = &renderTarget;
    m_curViewport = &viewport;
    m_curMask = mask;
    m_curRegion = region;
    m_curScreen = screen;
    const QSizeF logical = logicalSize(viewport);
    m_curPixelSize = QSizeF(std::round(logical.width() * viewport.scale()), std::round(logical.height() * viewport.scale()));

    OutputState &state = m_outputs[screen];
    if (!ensureFramebuffers(state, screen)) {
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }

    // 撮影内容が有効な間は再利用し、切替開始時や対象ウィンドウの構成変更時に撮り直す
    if (!state.capturesValid) {
        capturePass(state, Pass::From);
        capturePass(state, Pass::To);
        capturePass(state, Pass::Background);
        if (m_overlayEnabled && !m_overlayWindows.isEmpty()) {
            capturePass(state, Pass::Overlay);
        }
        state.capturesValid = true;
    }
    compositeScene(state);
}

/**
 * @brief 出力のオフスクリーンリソースを必要なサイズと形式で用意する
 * @param state 対象出力の状態
 * @param screen 対象の出力
 * @return 用意できたらtrue
 *
 * サイズが空、または確保に失敗したらfalseを返す
 * 形式かサイズが変わったときだけ作り直し、旧面と新面はミップマップ付きにする
 */
bool FacetsEffect::ensureFramebuffers(OutputState &state, ScreenOutput *screen)
{
    Q_UNUSED(screen)

    const QSize size = m_curPixelSize.toSize();
    if (size.isEmpty()) {
        return false;
    }
    const bool hdr = !(m_curRenderTarget->colorDescription() == ColorDescription::sRGB);
    if (state.fromTexture && state.size == size && state.hdr == hdr) {
        return true;
    }

    // 形式かサイズが変わったら作り直す
    const GLenum format = hdr ? GL_RGBA16F : GL_RGBA8;
    const auto create = [&](std::unique_ptr<GLTexture> &texture, std::unique_ptr<GLFramebuffer> &framebuffer, int levels) {
        texture = GLTexture::allocate(format, size, levels);
        if (!texture) {
            framebuffer.reset();
            return false;
        }
        texture->setFilter(levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        framebuffer = std::make_unique<GLFramebuffer>(texture.get());
        return framebuffer->valid();
    };

    state.fromTexture.reset();
    const bool ok = create(state.fromTexture, state.fromFramebuffer, kMipLevels)
        && create(state.toTexture, state.toFramebuffer, kMipLevels)
        && create(state.backgroundTexture, state.backgroundFramebuffer, 1)
        && create(state.overlayTexture, state.overlayFramebuffer, 1);
    if (!ok) {
        state.fromTexture.reset();
        return false;
    }
    state.size = size;
    state.hdr = hdr;
    state.capturesValid = false;
    return true;
}

/**
 * @brief 指定した面をオフスクリーンへ1パス分だけ描画する
 * @param state 対象出力の状態
 * @param pass 撮影する面
 *
 * FBOを積んだ状態で通常のpaintScreenを呼び、対象のウィンドウ集合を焼き付ける
 * 旧面と新面は回転で折り返しが出ないようミップマップを更新する
 */
void FacetsEffect::capturePass(OutputState &state, Pass pass)
{
    GLTexture *texture = nullptr;
    GLFramebuffer *framebuffer = nullptr;
    switch (pass) {
    case Pass::From:
        texture = state.fromTexture.get();
        framebuffer = state.fromFramebuffer.get();
        break;
    case Pass::To:
        texture = state.toTexture.get();
        framebuffer = state.toFramebuffer.get();
        break;
    case Pass::Background:
        texture = state.backgroundTexture.get();
        framebuffer = state.backgroundFramebuffer.get();
        break;
    case Pass::Overlay:
        texture = state.overlayTexture.get();
        framebuffer = state.overlayFramebuffer.get();
        break;
    case Pass::None:
        return;
    }

    RenderTarget offscreenTarget(framebuffer, m_curRenderTarget->colorDescription());
#if defined(FACETS_KWIN_NEW_API)
    // 6.5以降のRenderViewportは描画オフセット引数が必要
    RenderViewport offscreenViewport(m_curViewport->renderRect(), m_curViewport->scale(), offscreenTarget, QPoint(0, 0));
#else
    RenderViewport offscreenViewport(m_curViewport->renderRect(), m_curViewport->scale(), offscreenTarget);
#endif

    GLFramebuffer::pushFramebuffer(framebuffer);
    // X11の汎用描画は1フレームの最初のpaintScreenでしか消去しないので自前で消す
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_pass = pass;
    effects->paintScreen(offscreenTarget, offscreenViewport, m_curMask, DeviceRegion(m_curViewport->renderRect().toAlignedRect()), m_curScreen);
    m_pass = Pass::None;

    GLFramebuffer::popFramebuffer();

    if (pass == Pass::From || pass == Pass::To) {
        // 回転や縮小で折り返しが出ないようミップマップを更新する
        texture->bind();
        texture->generateMipmaps();
        texture->unbind();
    }
}

/**
 * @brief 現在の出力サイズに対するカメラの透視投影行列を作る
 * @return ビュー変換まで含めた投影行列
 *
 * z=0の面がちょうど画面いっぱいになる距離にカメラを置き、進捗の山で[ZoomOut]の分だけ引く
 * 立方体では中心を軸に[TiltAngle]の傾きを加える
 */
QMatrix4x4 FacetsEffect::cameraProjection() const
{
    const QSizeF size = logicalSize(*m_curViewport);
    const double width = size.width();
    const double height = size.height();
    const double p = std::clamp(m_progress, 0.0, 1.0);

    // z=0の面がちょうど画面いっぱいになる距離
    const double fitDistance = 0.5 * height / std::tan(toRadians(kFovDegrees * 0.5));
    const double zoom = 1.0 + (m_zoomOut / 100.0) * std::sin(kPi * p);
    const double cameraZ = fitDistance * zoom;

    const bool cube = styleFromConfig() == Style::Cube;
    const double radius = cube ? cubeRadius(size, m_vertical) : 0.0;
    const double tilt = m_tiltAngle * std::sin(kPi * p);

    QMatrix4x4 projection;
    projection.perspective(float(kFovDegrees), float(width / height), float(fitDistance * 0.05), float(cameraZ + 4.0 * std::max(width, height)));

    // y軸下向きの論理座標をy軸上向きのカメラ空間へ移す
    QMatrix4x4 view;
    view.scale(1.0f, -1.0f, 1.0f);
    view.translate(0.0f, 0.0f, float(-cameraZ));
    // 立方体の中心を軸に全体を少し傾ける
    view.translate(0.0f, 0.0f, float(-radius));
    if (m_vertical) {
        view.rotate(float(tilt * m_direction), 0.0f, 1.0f, 0.0f);
    } else {
        view.rotate(float(tilt), 1.0f, 0.0f, 0.0f);
    }
    view.translate(0.0f, 0.0f, float(radius));
    view.translate(float(-width * 0.5), float(-height * 0.5), 0.0f);

    return projection * view;
}

/**
 * @brief 進捗から1枚の面の変換行列と明るさを求める
 * @param style トランジションの見た目
 * @param toFace 新デスクトップの面か(falseは旧面)
 * @param progress 進捗(0.0〜1.0)
 * @return 面のモデル行列と明るさ
 */
FacetsEffect::FaceState FacetsEffect::faceState(Style style, bool toFace, qreal progress) const
{
    const QSizeF size = logicalSize(*m_curViewport);
    const double width = size.width();
    const double height = size.height();
    const double p = std::clamp(progress, 0.0, 1.0);
    const double s = m_direction;

    FaceState face;
    QMatrix4x4 &m = face.matrix;
    m.translate(float(width * 0.5), float(height * 0.5), 0.0f);

    switch (style) {
    case Style::Cube: {
        const double radius = cubeRadius(size, m_vertical);
        m.translate(0.0f, 0.0f, float(-radius));
        if (!m_vertical) {
            m.rotate(float(-90.0 * p * s), 0.0f, 1.0f, 0.0f);
            if (toFace) {
                m.translate(float(s * radius), 0.0f, 0.0f);
                m.rotate(float(90.0 * s), 0.0f, 1.0f, 0.0f);
            } else {
                m.translate(0.0f, 0.0f, float(radius));
            }
        } else {
            m.rotate(float(90.0 * p * s), 1.0f, 0.0f, 0.0f);
            if (toFace) {
                m.translate(0.0f, float(s * radius), 0.0f);
                m.rotate(float(-90.0 * s), 1.0f, 0.0f, 0.0f);
            } else {
                m.translate(0.0f, 0.0f, float(radius));
            }
        }
        const double angle = toFace ? 90.0 * (1.0 - p) : 90.0 * p;
        face.brightness = m_shading ? 0.35 + 0.65 * std::cos(toRadians(angle)) : 1.0;
        break;
    }
    case Style::Flip: {
        // カードを180度回し、裏側に来る新面は鏡像を打ち消すため反転させる
        if (!m_vertical) {
            m.rotate(float(-180.0 * p * s), 0.0f, 1.0f, 0.0f);
            if (toFace) {
                m.scale(-1.0f, 1.0f, 1.0f);
            }
            face.brightness = m_shading ? 0.35 + 0.65 * std::abs(std::cos(toRadians(180.0 * p))) : 1.0;
        } else {
            m.rotate(float(180.0 * p * s), 1.0f, 0.0f, 0.0f);
            if (toFace) {
                m.scale(1.0f, -1.0f, 1.0f);
            }
            face.brightness = m_shading ? 0.35 + 0.65 * std::abs(std::cos(toRadians(180.0 * p))) : 1.0;
        }
        break;
    }
    case Style::Deck: {
        const double dimension = m_vertical ? height : width;
        if (!toFace) {
            // 旧面は奥へ下がりながら進行と逆方向へ少しずれて暗くなる
            const double shift = -s * 0.12 * dimension * p;
            m.translate(float(m_vertical ? 0.0 : shift), float(m_vertical ? shift : 0.0), float(-0.2 * dimension * p));
            const double scale = 1.0 - 0.08 * p;
            m.scale(float(scale), float(scale), 1.0f);
            face.brightness = m_shading ? 1.0 - 0.55 * p : 1.0;
        } else {
            const double offset = s * dimension * (1.0 - p);
            m.translate(float(m_vertical ? 0.0 : offset), float(m_vertical ? offset : 0.0), 0.0f);
            face.brightness = 1.0;
        }
        break;
    }
    }
    return face;
}

/**
 * @brief 背景・面・オーバーレイを合成して実レンダーターゲットへ出力する
 * @param state 対象出力の状態
 *
 * 面は深度バッファが無いため奥から手前へ並べ替えて描く(画家のアルゴリズム)
 * [Flip]では表裏のうち手前を向いている一枚だけを描く
 * 乗算済みアルファで描くためブレンドはGL_ONE / GL_ONE_MINUS_SRC_ALPHAにする
 */
void FacetsEffect::compositeScene(OutputState &state)
{
    const RenderTarget &renderTarget = *m_curRenderTarget;
    const QSizeF size = logicalSize(*m_curViewport);
    const double width = size.width();
    const double height = size.height();
    const double p = std::clamp(m_progress, 0.0, 1.0);
    const Style style = styleFromConfig();

    // 出力transformはorthoの後段(NDC)に掛かるので最後に左から合成する
    QMatrix4x4 outputTransform;
    outputTransform.scale(1.0f, -1.0f);
    outputTransform *= renderTarget.transform().toMatrix();
    outputTransform.scale(1.0f, -1.0f);

    QMatrix4x4 flat;
    flat.ortho(0.0f, float(width), float(height), 0.0f, -1.0f, 1.0f);
    flat = outputTransform * flat;

    const std::array<QVector3D, 4> screenCorners{
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(float(width), 0.0f, 0.0f),
        QVector3D(float(width), float(height), 0.0f),
        QVector3D(0.0f, float(height), 0.0f),
    };
    const std::array<QVector3D, 4> faceCorners{
        QVector3D(float(-width * 0.5), float(-height * 0.5), 0.0f),
        QVector3D(float(width * 0.5), float(-height * 0.5), 0.0f),
        QVector3D(float(width * 0.5), float(height * 0.5), 0.0f),
        QVector3D(float(-width * 0.5), float(height * 0.5), 0.0f),
    };

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // 背景:新デスクトップの壁紙を減光して敷く
    QuadDraw background;
    background.mvp = flat;
    background.corners = screenCorners;
    background.texture = state.backgroundTexture.get();
    background.brightness = float(std::clamp(1.0 - m_backgroundDim / 100.0, 0.0, 1.0));
    drawQuad(background);

    // 面:奥から手前へ描く(深度バッファが無いため画家のアルゴリズム)
    const QMatrix4x4 camera = outputTransform * cameraProjection();
    /**
     * @struct Face
     * @brief 描画順の決定に使う面の変換・テクスチャ・奥行き
     */
    struct Face {
        FaceState state;    // 面の変換と明るさ
        GLTexture *texture; // 描画するテクスチャ
        double depth;       // モデル変換後の面中心の奥行き
    };
    std::vector<Face> faces;
    const auto add = [&](bool toFace) {
        Face face;
        face.state = faceState(style, toFace, p);
        face.texture = toFace ? state.toTexture.get() : state.fromTexture.get();
        face.depth = face.state.matrix.map(QVector3D(0.0f, 0.0f, 0.0f)).z();
        faces.push_back(std::move(face));
    };
    if (style == Style::Flip) {
        // カードは表裏のうち手前を向いている一枚だけ
        add(p >= 0.5);
    } else {
        add(false);
        add(true);
    }
    std::stable_sort(faces.begin(), faces.end(), [](const Face &a, const Face &b) {
        return a.depth < b.depth;
    });

    // 画面端が出ていない静止状態では縁の溶け込みを抑える
    const float aaStrength = float(std::clamp(8.0 * std::sin(kPi * p), 0.0, 1.0));
    for (const Face &face : faces) {
        QuadDraw draw;
        draw.mvp = camera * face.state.matrix;
        draw.corners = faceCorners;
        draw.texture = face.texture;
        draw.brightness = float(face.state.brightness);
        draw.aaStrength = aaStrength;
        drawQuad(draw);
    }

    // オーバーレイ:パネルなどは面の上に等倍で重ねる
    if (m_overlayEnabled && !m_overlayWindows.isEmpty()) {
        QuadDraw overlay;
        overlay.mvp = flat;
        overlay.corners = screenCorners;
        overlay.texture = state.overlayTexture.get();
        drawQuad(overlay);
    }

    glDisable(GL_BLEND);
}

/**
 * @brief シェーダを読み込んでuniform位置を解決する
 *
 * シェーダが無効なら破棄して今回の描画を省略し、次の描画要求で再試行する
 */
void FacetsEffect::resolveUniforms()
{
    if (!m_shader) {
        m_shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                      QString::fromLatin1(kVertexShader),
                                                                      QString::fromLatin1(kFragmentShader));
    }
#if defined(FACETS_KWIN_NEW_API)
    // 6.5以降のgenerateShaderFromFile()は読み込み/リンク失敗時にnullptrを返す
    if (!m_shader) {
        return;
    }
#else
    if (!m_shader || !m_shader->isValid()) {
        m_shader.reset();
        return;
    }
#endif
    m_mvpLocation = m_shader->uniformLocation("u_mvp");
    m_textureLocation = m_shader->uniformLocation("u_texture");
    m_brightnessLocation = m_shader->uniformLocation("u_brightness");
    m_opacityLocation = m_shader->uniformLocation("u_opacity");
    m_aaLocation = m_shader->uniformLocation("u_aaStrength");
}

/**
 * @brief 1枚のクアッドを描画する
 * @param draw 描画パラメータ
 *
 * 頂点は左上・右上・右下・左下の順で2三角形に組み、テクスチャは上端v=1下端v=0で貼る
 */
void FacetsEffect::drawQuad(const QuadDraw &draw)
{
    if (!draw.texture) {
        return;
    }
    if (!m_shader) {
        resolveUniforms();
        if (!m_shader) {
            return;
        }
    }

    // 上端がv=1、下端がv=0
    const float u0 = draw.mirrorU ? 1.0f : 0.0f;
    const float u1 = draw.mirrorU ? 0.0f : 1.0f;
    const float vTop = draw.mirrorV ? 0.0f : 1.0f;
    const float vBottom = draw.mirrorV ? 1.0f : 0.0f;
    const GLVertex3D topLeft{draw.corners[0], QVector2D(u0, vTop)};
    const GLVertex3D topRight{draw.corners[1], QVector2D(u1, vTop)};
    const GLVertex3D bottomRight{draw.corners[2], QVector2D(u1, vBottom)};
    const GLVertex3D bottomLeft{draw.corners[3], QVector2D(u0, vBottom)};

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex3DLayout), sizeof(GLVertex3D));
    const auto map = vbo->map<GLVertex3D>(6);
    if (!map) {
        return;
    }
    GLVertex3D *vertices = map->data();
    vertices[0] = topLeft;
    vertices[1] = topRight;
    vertices[2] = bottomRight;
    vertices[3] = topLeft;
    vertices[4] = bottomRight;
    vertices[5] = bottomLeft;
    vbo->unmap();

    ShaderManager::instance()->pushShader(m_shader.get());
    m_shader->setUniform(m_mvpLocation, draw.mvp);
    m_shader->setUniform(m_textureLocation, 0);
    m_shader->setUniform(m_brightnessLocation, draw.brightness);
    m_shader->setUniform(m_opacityLocation, draw.opacity);
    m_shader->setUniform(m_aaLocation, draw.aaStrength);

    draw.texture->bind();
    vbo->bindArrays();
    vbo->draw(GL_TRIANGLES, 0, 6);
    vbo->unbindArrays();
    draw.texture->unbind();

    ShaderManager::instance()->popShader();
}

} // 名前空間の終わり
