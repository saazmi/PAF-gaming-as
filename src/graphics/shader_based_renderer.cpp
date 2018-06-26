//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2015 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef SERVER_ONLY

#include "graphics/shader_based_renderer.hpp"

#include "config/user_config.hpp"
#include "graphics/camera.hpp"
#include "graphics/central_settings.hpp"
#include "graphics/cpu_particle_manager.hpp"
#include "graphics/frame_buffer_layer.hpp"
#include "graphics/glwrap.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/lod_node.hpp"
#include "graphics/post_processing.hpp"
#include "graphics/render_target.hpp"
#include "graphics/rtts.hpp"
#include "graphics/shaders.hpp"
#include "graphics/skybox.hpp"
#include "graphics/spherical_harmonics.hpp"
#include "graphics/sp/sp_base.hpp"
#include "graphics/sp/sp_shader.hpp"
#include "graphics/texture_shader.hpp"
#include "graphics/text_billboard_drawer.hpp"
#include "items/item_manager.hpp"
#include "items/powerup_manager.hpp"
#include "modes/world.hpp"
#include "physics/physics.hpp"
#include "states_screens/race_gui_base.hpp"
#include "tracks/track.hpp"
#include "utils/profiler.hpp"

#include "../../lib/irrlicht/source/Irrlicht/CCameraSceneNode.h"
#include "../../lib/irrlicht/source/Irrlicht/CSceneManager.h"
#include "../../lib/irrlicht/source/Irrlicht/os.h"
#include <algorithm>

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::setRTT(RTT* rtts)
{
    m_rtts = rtts;
} //setRTT

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::prepareForwardRenderer()
{
    irr::video::SColor clearColor(0, 150, 150, 150);
    if (World::getWorld() != NULL)
        clearColor = irr_driver->getClearColor();

    glClear(GL_COLOR_BUFFER_BIT);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(clearColor.getRed() / 255.f, clearColor.getGreen() / 255.f,
        clearColor.getBlue() / 255.f, clearColor.getAlpha() / 255.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// ----------------------------------------------------------------------------
/** Upload lighting info to the dedicated uniform buffer
 */
void ShaderBasedRenderer::uploadLightingData() const
{
    assert(CVS->isARBUniformBufferObjectUsable());

    float Lighting[36];

    core::vector3df sun_direction = irr_driver->getSunDirection();
    video::SColorf sun_color = irr_driver->getSunColor();

    Lighting[0] = sun_direction.X;
    Lighting[1] = sun_direction.Y;
    Lighting[2] = sun_direction.Z;
    Lighting[4] = sun_color.getRed();
    Lighting[5] = sun_color.getGreen();
    Lighting[6] = sun_color.getBlue();
    Lighting[7] = 0.54f;

    const SHCoefficients* sh_coeff = m_spherical_harmonics->getCoefficients();

    if(sh_coeff) {
        memcpy(&Lighting[8], sh_coeff->blue_SH_coeff, 9 * sizeof(float));
        memcpy(&Lighting[17], sh_coeff->green_SH_coeff, 9 * sizeof(float));
        memcpy(&Lighting[26], sh_coeff->red_SH_coeff, 9 * sizeof(float));
    }

    glBindBuffer(GL_UNIFORM_BUFFER, SharedGPUObjects::getLightingDataUBO());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 36 * sizeof(float), Lighting);
}   // uploadLightingData


// ----------------------------------------------------------------------------
void ShaderBasedRenderer::computeMatrixesAndCameras(scene::ICameraSceneNode *const camnode,
                                                    unsigned int width, unsigned int height)
{
    m_current_screen_size = core::vector2df((float)width, (float)height);
    m_shadow_matrices.computeMatrixesAndCameras(camnode, width, height);
}   // computeMatrixesAndCameras

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderSkybox(const scene::ICameraSceneNode *camera) const
{
    if(m_skybox)
    {
        m_skybox->render(camera);
    }
}   // renderSkybox

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderSSAO() const
{
    if (!CVS->isEXTColorBufferFloatUsable())
        return;

    m_rtts->getFBO(FBO_SSAO).bind();
    glClearColor(1., 1., 1., 1.);
    glClear(GL_COLOR_BUFFER_BIT);
    m_post_processing->renderSSAO(m_rtts->getFBO(FBO_LINEAR_DEPTH),
                                  m_rtts->getFBO(FBO_SSAO),
                                  m_rtts->getDepthStencilTexture());
    // Blur it to reduce noise.
    FrameBuffer::blit(m_rtts->getFBO(FBO_SSAO),
                      m_rtts->getFBO(FBO_HALF1_R),
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    m_post_processing->renderGaussian17TapBlur(m_rtts->getFBO(FBO_HALF1_R),
                                               m_rtts->getFBO(FBO_HALF2_R),
                                               m_rtts->getFBO(FBO_LINEAR_DEPTH));

}   // renderSSAO

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderGlow() const
{
    irr_driver->getSceneManager()->setCurrentRendertime(scene::ESNRP_SOLID);
    m_rtts->getFBO(FBO_RGBA_2).bind();
    glClearStencil(0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, ~0);
    glEnable(GL_STENCIL_TEST);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    SP::drawGlow();

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDisable(GL_STENCIL_TEST);
}   // renderGlow

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderShadows()
{
    PROFILER_PUSH_CPU_MARKER("- Shadow Pass", 0xFF, 0x00, 0x00);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 50.0f);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    for (unsigned cascade = 0; cascade < 4; cascade++)
    {
        m_rtts->getShadowFrameBuffer()->bindLayerDepthOnly(cascade);
        glClear(GL_DEPTH_BUFFER_BIT);
        SP::sp_cur_shadow_cascade = cascade;
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SHADOWS_CASCADE0 + cascade));
        SP::draw(SP::RP_SHADOW, (SP::DrawCallType)(SP::DCT_SHADOW1 + cascade));
    }
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    PROFILER_POP_CPU_MARKER();
}

// ============================================================================
class CombineDiffuseColor : public TextureShader<CombineDiffuseColor, 7,
                                                 std::array<float, 4> >
{
public:
    CombineDiffuseColor()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "screenquad.vert",
                            GL_FRAGMENT_SHADER, "combine_diffuse_color.frag");
        assignUniforms("bg_color");
        assignSamplerNames(0, "diffuse_map", ST_NEAREST_FILTERED,
                           1, "specular_map", ST_NEAREST_FILTERED,
                           2, "ssao_tex", ST_NEAREST_FILTERED,
                           3, "normal_color", ST_NEAREST_FILTERED,
                           4, "diffuse_color", ST_NEAREST_FILTERED,
                           5, "depth_stencil", ST_NEAREST_FILTERED,
                           6, "light_scatter", ST_NEAREST_FILTERED);
    }   // CombineDiffuseColor
    // ------------------------------------------------------------------------
    void render(GLuint dm, GLuint sm, GLuint st, GLuint nt, GLuint dc,
                GLuint ds, GLuint lt, const std::array<float, 4> & bg_color)
    {
        setTextureUnits(dm, sm, st, nt, dc, ds, lt);
        drawFullScreenEffect(bg_color);
    }   // render
};   // CombineDiffuseColor


// ============================================================================
class MultiViewShader: public TextureShader<MultiViewShader, 6, int, int >
{
public:
	MultiViewShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "multiview.vert",
                            GL_FRAGMENT_SHADER, "multiview.frag");
        assignUniforms("bg_color","nb_views");



        assignSamplerNames(0, "texture_j1g", ST_NEAREST_FILTERED,
                           1, "texture_j1c", ST_NEAREST_FILTERED,
                           2, "texture_j1d", ST_NEAREST_FILTERED,
                           3, "texture_j2g", ST_NEAREST_FILTERED,
                           4, "texture_j2c", ST_NEAREST_FILTERED,
                           5, "texture_j2d", ST_NEAREST_FILTERED);
   }
};   // MultiViewShader


// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderSceneDeferred(scene::ICameraSceneNode * const camnode,
                                              float dt,
                                              bool hasShadow,
                                              bool forceRTT)
{

    if (CVS->isARBUniformBufferObjectUsable())
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0,
            SP::sp_mat_ubo[SP::sp_cur_player][SP::sp_cur_buf_id[SP::sp_cur_player]]);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, SharedGPUObjects::getLightingDataUBO());
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, SP::sp_fog_ubo);
    }
    irr_driver->getSceneManager()->setActiveCamera(camnode);

    PROFILER_PUSH_CPU_MARKER("- Draw Call Generation", 0xFF, 0xFF, 0xFF);
    m_draw_calls.prepareDrawCalls(camnode);
    PROFILER_POP_CPU_MARKER();
    PROFILER_PUSH_CPU_MARKER("Update Light Info", 0xFF, 0x0, 0x0);
    m_lighting_passes.updateLightsInfo(camnode, dt);
    PROFILER_POP_CPU_MARKER();

    // Shadows
    if (CVS->isShadowEnabled() && hasShadow)
    {
        renderShadows();
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    {
        m_rtts->getFBO(FBO_SP).bind();
        float clear_color_empty[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, clear_color_empty);
        glClearBufferfv(GL_COLOR, 1, clear_color_empty);
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SOLID_PASS));
        SP::draw(SP::RP_1ST, SP::DCT_NORMAL);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    // Lights
    {
        PROFILER_PUSH_CPU_MARKER("- Light", 0x00, 0xFF, 0x00);
        m_rtts->getFBO(FBO_COMBINED_DIFFUSE_SPECULAR).bind();
        glClear(GL_COLOR_BUFFER_BIT);
        GLuint specular_probe = 0;
        if (m_skybox)
        {
            specular_probe = m_skybox->getSpecularProbe();
        }

        m_lighting_passes.renderLights( hasShadow,
                                        m_rtts->getRenderTarget(RTT_NORMAL_AND_DEPTH),
                                        m_rtts->getDepthStencilTexture(),
                                        m_rtts->getShadowFrameBuffer(),
                                        specular_probe);
        PROFILER_POP_CPU_MARKER();
    }

    ///travail
    // Handle SSAO
    {
        PROFILER_PUSH_CPU_MARKER("- SSAO", 0xFF, 0xFF, 0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SSAO));
        if (UserConfigParams::m_ssao)
            renderSSAO();
        PROFILER_POP_CPU_MARKER();
    }

    const Track * const track = Track::getCurrentTrack();
    // Render discrete lights scattering
    if (track && track->isFogEnabled())
    {
        PROFILER_PUSH_CPU_MARKER("- PointLight Scatter", 0xFF, 0x00, 0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_LIGHTSCATTER));
        m_lighting_passes.renderLightsScatter(m_rtts->getDepthStencilTexture(),
                                              m_rtts->getFBO(FBO_HALF1),
                                              m_rtts->getFBO(FBO_HALF2),
                                              m_post_processing);
        PROFILER_POP_CPU_MARKER();
    }

    ///travail
    // Render anything glowing.
    if (UserConfigParams::m_glow)
    {
        PROFILER_PUSH_CPU_MARKER("- Glow", 0xFF, 0xFF, 0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_GLOW));
        renderGlow();
        // To half
        FrameBuffer::blit(m_rtts->getFBO(FBO_RGBA_2),
            m_rtts->getFBO(FBO_HALF2), GL_COLOR_BUFFER_BIT, GL_LINEAR);
        // To quarter
        FrameBuffer::blit(m_rtts->getFBO(FBO_HALF2),
            m_rtts->getFBO(FBO_QUARTER1), GL_COLOR_BUFFER_BIT, GL_LINEAR);
        PROFILER_POP_CPU_MARKER();
    } // end glow

    m_rtts->getFBO(FBO_COLORS).bind();
    glClear(UserConfigParams::m_glow ? GL_COLOR_BUFFER_BIT :
        GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    {
        PROFILER_PUSH_CPU_MARKER("- Combine diffuse color", 0x2F, 0x77, 0x33);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_COMBINE_DIFFUSE_COLOR));
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        std::array<float, 4> bg_color = {{ 1.0f, 1.0f, 1.0f, 0.0f }};
        if (World::getWorld() != NULL)
        {
            bg_color[0] =
                irr_driver->getClearColor().getRed() / 255.0f;
            bg_color[1] =
                irr_driver->getClearColor().getGreen() / 255.0f;
            bg_color[2] =
                irr_driver->getClearColor().getBlue() / 255.0f;
            bg_color[3] =
                irr_driver->getClearColor().getAlpha() / 255.0f;
        }

        CombineDiffuseColor::getInstance()->render(
            m_rtts->getRenderTarget(RTT_DIFFUSE),
            m_rtts->getRenderTarget(RTT_SPECULAR),
            m_rtts->getRenderTarget(RTT_HALF1_R),
            m_rtts->getRenderTarget(RTT_NORMAL_AND_DEPTH),
            m_rtts->getRenderTarget(RTT_SP_DIFF_COLOR),
            m_rtts->getDepthStencilTexture(),
            m_rtts->getRenderTarget(RTT_HALF1), !m_skybox ?
            bg_color : std::array<float, 4>{{0.0f, 0.0f, 0.0f, 0.0f}});
        PROFILER_POP_CPU_MARKER();
    }

    if (SP::sp_debug_view)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        m_rtts->getFBO(FBO_NORMAL_AND_DEPTHS).bind();
        SP::drawSPDebugView();
        m_rtts->getFBO(FBO_COLORS).bind();
    }

    {
        PROFILER_PUSH_CPU_MARKER("- Skybox", 0xFF, 0x00, 0xFF);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SKYBOX));
        renderSkybox(camnode);
        PROFILER_POP_CPU_MARKER();
    }

    if (UserConfigParams::m_glow)
    {
        m_post_processing->renderGlow(m_rtts->getFBO(FBO_QUARTER1));
    }

    // Render transparent
    {
        PROFILER_PUSH_CPU_MARKER("- Transparent Pass", 0xFF, 0x00, 0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_TRANSPARENT));
        SP::draw(SP::RP_1ST, SP::DCT_TRANSPARENT);
        SP::draw(SP::RP_RESERVED, SP::DCT_TRANSPARENT);
        PROFILER_POP_CPU_MARKER();
    }

    // Render particles and text billboard
    {
        PROFILER_PUSH_CPU_MARKER("- Particles and text billboard", 0xFF, 0xFF,
            0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_PARTICLES));
        CPUParticleManager::getInstance()->drawAll();
        TextBillboardDrawer::drawAll();
        PROFILER_POP_CPU_MARKER();
    }
    glDisable(GL_CULL_FACE);

    // Now all instancing data from mesh and particle are done drawing
    m_draw_calls.setFenceSync();

    if (!forceRTT)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

} //renderSceneDeferred

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderScene(scene::ICameraSceneNode * const camnode,
                                      float dt,
                                      bool hasShadow,
                                      bool forceRTT)
{
    if (CVS->isARBUniformBufferObjectUsable())
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0,
            SP::sp_mat_ubo[SP::sp_cur_player][SP::sp_cur_buf_id[SP::sp_cur_player]]);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, SharedGPUObjects::getLightingDataUBO());
    }
    irr_driver->getSceneManager()->setActiveCamera(camnode);

    PROFILER_PUSH_CPU_MARKER("- Draw Call Generation", 0xFF, 0xFF, 0xFF);
    m_draw_calls.prepareDrawCalls(camnode);
    PROFILER_POP_CPU_MARKER();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    if (forceRTT)
    {
        m_rtts->getFBO(FBO_COLORS).bind();
        video::SColor clearColor(0, 150, 150, 150);
        if (World::getWorld() != NULL)
            clearColor = irr_driver->getClearColor();

        glClearColor(clearColor.getRed() / 255.f, clearColor.getGreen() / 255.f,
            clearColor.getBlue() / 255.f, clearColor.getAlpha() / 255.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SOLID_PASS));
        SP::draw(SP::RP_1ST, SP::DCT_NORMAL);
    }

    {
        PROFILER_PUSH_CPU_MARKER("- Skybox", 0xFF, 0x00, 0xFF);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_SKYBOX));
        renderSkybox(camnode);
        PROFILER_POP_CPU_MARKER();
    }

    // Render transparent
    {
        PROFILER_PUSH_CPU_MARKER("- Transparent Pass", 0xFF, 0x00, 0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_TRANSPARENT));
        SP::draw(SP::RP_1ST, SP::DCT_TRANSPARENT);
        SP::draw(SP::RP_RESERVED, SP::DCT_TRANSPARENT);
        PROFILER_POP_CPU_MARKER();
    }

    // Render particles and text billboard
    {
        PROFILER_PUSH_CPU_MARKER("- Particles and text billboard", 0xFF, 0xFF,
            0x00);
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_PARTICLES));
        CPUParticleManager::getInstance()->drawAll();
        TextBillboardDrawer::drawAll();
        PROFILER_POP_CPU_MARKER();
    }
    glDisable(GL_CULL_FACE);

    // Now all instancing data from mesh and particle are done drawing
    m_draw_calls.setFenceSync();

    if (!forceRTT)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    glBindVertexArray(0);

} //renderScene

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::debugPhysics()
{
    // Note that drawAll must be called before rendering
    // the bullet debug view, since otherwise the camera
    // is not set up properly. This is only used for
    // the bullet debug view.
    if(Physics::getInstance())
    {
        if (UserConfigParams::m_artist_debug_mode)
            Physics::getInstance()->draw();

        IrrDebugDrawer* debug_drawer = Physics::getInstance()->getDebugDrawer();
        if (debug_drawer != NULL && debug_drawer->debugEnabled())
        {
            const std::map<video::SColor, std::vector<float> >& lines =
                                                       debug_drawer->getLines();
            std::map<video::SColor, std::vector<float> >::const_iterator it;

            glEnable(GL_DEPTH_TEST);
            Shaders::ColoredLine *line = Shaders::ColoredLine::getInstance();
            line->use();
            line->bindVertexArray();
            line->bindBuffer();
            for (it = lines.begin(); it != lines.end(); it++)
            {
                line->setUniforms(it->first);
                const std::vector<float> &vertex = it->second;
                const float *tmp = vertex.data();
                for (unsigned int i = 0; i < vertex.size(); i += 1024 * 6)
                {
                    unsigned count = std::min((unsigned)vertex.size() - i, (unsigned)1024 * 6);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(float), &tmp[i]);

                    glDrawArrays(GL_LINES, 0, count / 3);
                }
            }
            glDisable(GL_DEPTH_TEST);
            glUseProgram(0);
            glBindVertexArray(0);
        }
    }
} //debugPhysics

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderPostProcessing(Camera * const camera,
                                               bool first_cam)
{
    scene::ICameraSceneNode * const camnode = camera->getCameraSceneNode();
    const core::recti &viewport = camera->getViewport();

    bool isRace = StateManager::get()->getGameState() == GUIEngine::GAME;
    FrameBuffer *fbo = m_post_processing->render(camnode, isRace, m_rtts);

    // The viewport has been changed using glViewport function directly
    // during scene rendering, but irrlicht thinks that nothing changed
    // when single camera is used. In this case we set the viewport
    // to whole screen manually.
    glViewport(0, 0, irr_driver->getActualScreenSize().Width,
        irr_driver->getActualScreenSize().Height);

    if (SP::sp_debug_view)
    {
        m_rtts->getFBO(FBO_NORMAL_AND_DEPTHS).blitToDefault(
            viewport.UpperLeftCorner.X,
            irr_driver->getActualScreenSize().Height - viewport.LowerRightCorner.Y,
            viewport.LowerRightCorner.X,
            irr_driver->getActualScreenSize().Height - viewport.UpperLeftCorner.Y);
    }
    else if (irr_driver->getSSAOViz())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (first_cam)
        {
             glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        camera->activate();
        m_post_processing->renderPassThrough(m_rtts->getFBO(FBO_HALF1_R).getRTT()[0], viewport.LowerRightCorner.X - viewport.UpperLeftCorner.X, viewport.LowerRightCorner.Y - viewport.UpperLeftCorner.Y);
    }
    else if (irr_driver->getShadowViz() && m_rtts->getShadowFrameBuffer())
    {
        m_shadow_matrices.renderShadowsDebug(m_rtts->getShadowFrameBuffer(), m_post_processing);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (first_cam)
        {
             glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        camera->activate();
        m_post_processing->renderPassThrough(fbo->getRTT()[0], viewport.LowerRightCorner.X - viewport.UpperLeftCorner.X, viewport.LowerRightCorner.Y - viewport.UpperLeftCorner.Y);
    }
    glBindVertexArray(0);
} //renderPostProcessing

// ----------------------------------------------------------------------------
ShaderBasedRenderer::ShaderBasedRenderer()
{
    m_rtts                  = NULL;
    m_skybox                = NULL;
    m_spherical_harmonics   = new SphericalHarmonics(irr_driver->getAmbientLight().toSColor());
    SharedGPUObjects::init();
    SP::init();
    SP::initSTKRenderer(this);
    m_post_processing = new PostProcessing();

    if (UserConfigParams::m_nb_views) {
    	fprintf(stderr, "nb views %d!", UserConfigParams::m_nb_views);
    }

    for (unsigned int i=0;i<8;i++) {
    	views_tx_objects[i]=NULL;
    }
}

// ----------------------------------------------------------------------------
ShaderBasedRenderer::~ShaderBasedRenderer()
{
    delete m_post_processing;
    delete m_spherical_harmonics;
    delete m_skybox;
    delete m_rtts;
    ShaderBase::killShaders();
    SP::destroy();
    ShaderFilesManager::kill();
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::onLoadWorld()
{
    const core::recti &viewport = Camera::getCamera(0)->getViewport();
    unsigned int width = viewport.LowerRightCorner.X - viewport.UpperLeftCorner.X;
    unsigned int height = viewport.LowerRightCorner.Y - viewport.UpperLeftCorner.Y;
    RTT* rtts = new RTT(width, height, CVS->isDeferredEnabled() ?
                        UserConfigParams::m_scale_rtts_factor : 1.0f,
                        !CVS->isDeferredEnabled());
    setRTT(rtts);
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::onUnloadWorld()
{
    delete m_rtts;
    m_rtts = NULL;
    removeSkyBox();
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::resetPostProcessing()
{
    if (CVS->isARBUniformBufferObjectUsable())
    {
        uploadLightingData();
    }
    m_post_processing->reset();
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::giveBoost(unsigned int cam_index)
{
    m_post_processing->giveBoost(cam_index);
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::addSkyBox(const std::vector<video::ITexture*> &texture,
                                    const std::vector<video::ITexture*> &spherical_harmonics_textures)
{
    m_skybox = new Skybox(texture);
    if(spherical_harmonics_textures.size() == 6)
    {
        m_spherical_harmonics->setTextures(spherical_harmonics_textures);
    }

    printf("texture");
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::removeSkyBox()
{
    delete m_skybox;
    m_skybox = NULL;
}

// ----------------------------------------------------------------------------
const SHCoefficients* ShaderBasedRenderer::getSHCoefficients() const
{
    return m_spherical_harmonics->getCoefficients();
}

// ----------------------------------------------------------------------------
GLuint ShaderBasedRenderer::getRenderTargetTexture(TypeRTT which) const
{
    return m_rtts->getRenderTarget(which);
}

// ----------------------------------------------------------------------------
GLuint ShaderBasedRenderer::getDepthStencilTexture() const
{
    return m_rtts->getDepthStencilTexture();
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::setAmbientLight(const video::SColorf &light,
                                          bool force_SH_computation)
{
    if (force_SH_computation)
        m_spherical_harmonics->setAmbientLight(light.toSColor());
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::addSunLight(const core::vector3df &pos)
{
    m_shadow_matrices.addLight(pos);
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::render(float dt)
{
    // Start the RTT for post-processing.
    // We do this before beginScene() because we want to capture the glClear()
    // because of tracks that do not have skyboxes (generally add-on tracks)
    m_post_processing->begin();

    World *world = World::getWorld(); // Never NULL.
    Track *track = Track::getCurrentTrack();

    RaceGUIBase *rg = world->getRaceGUI();
    if (rg) rg->update(dt);

    if (!CVS->isDeferredEnabled())
    {
        prepareForwardRenderer();
    }

    {
        PROFILER_PUSH_CPU_MARKER("Update scene", 0x0, 0xFF, 0x0);
        static_cast<scene::CSceneManager *>(irr_driver->getSceneManager())
            ->OnAnimate(os::Timer::getTime());
        PROFILER_POP_CPU_MARKER();
    }

    //GL3RenderTarget * renderTarget;
    assert(Camera::getNumCameras() < MAX_PLAYER_COUNT + 1);


    ///LA BOUCLE !!! THE ONE !!!!!

    for(unsigned int cam = 0; cam < Camera::getNumCameras(); cam++)
    {
      unsigned int i;
      for (i=0; i < 3;i++) {

        char buffer[100];
        sprintf(buffer,"target%d",cam*3+i);
        if (!views_tx_objects[3*(cam)+i])
          {
            views_tx_objects[3*(cam)+i]=new GL3RenderTarget(irr::core::dimension2du(m_rtts->getWidth(), m_rtts->getHeight()),buffer,this);
          }
    //    GL3RenderTarget * renderTarget = createRenderTarget(irr::core::dimension2du(m_rtts->getWidth(), m_rtts->getHeight(),buffer);
      //  renderTarget=views_tx_objects[2*(cam)+i];
        SP::sp_cur_player = cam;
        SP::sp_cur_buf_id[cam] = (SP::sp_cur_buf_id[cam] + 1) % 3;
        Camera * const camera = Camera::getCamera(cam);
        scene::ICameraSceneNode * camnode = camera->getCameraSceneNode();

        camera->activate(!CVS->isDeferredEnabled());
        rg->preRenderCallback(camera);   // adjusts start referee (feu rouge, vert...)
        //std::ostringstream oss;


        if (i==0) {
        camnode->setShift(-0.125);}
        else if (i==2){camnode->setShift(0.125);}
        irr_driver->getSceneManager()->setActiveCamera(camnode);
        computeMatrixesAndCameras(camnode, m_rtts->getWidth(), m_rtts->getHeight());

        // Save projection-view matrix for the next frame
        camera->setPreviousPVMatrix(irr_driver->getProjViewMatrix());





        views_tx_objects[3*(cam)+i]->renderToTexture(camnode,dt);



/*
        std::ostringstream oss;
        oss << "drawAll() for kart " << cam;
        PROFILER_PUSH_CPU_MARKER(oss.str().c_str(), (cam+1)*60,
                                 0x00, 0x00);
        camera->activate(!CVS->isDeferredEnabled());
        rg->preRenderCallback(camera);   // adjusts start referee
        irr_driver->getSceneManager()->setActiveCamera(camnode);

        computeMatrixesAndCameras(camnode, m_rtts->getWidth(), m_rtts->getHeight());
        if (CVS->isDeferredEnabled())
        {
            renderSceneDeferred(camnode, dt, track->hasShadows(), false);
        }
        else
        {
            renderScene(camnode, dt, track->hasShadows(), false);
        }

        if (irr_driver->getBoundingBoxesViz())
        {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            SP::drawBoundingBoxes();
            m_draw_calls.renderBoundingBoxes();
        }

        debugPhysics();

        if (CVS->isDeferredEnabled())
        {
            renderPostProcessing(camera, cam == 0);//cam == 1 affiche que la vue du joueur 2
        }
*/
}
      }
  //}
      // for i<world->getNumKarts()
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);




///retire la grille d'info de là ...
/*
    // Set the viewport back to the full screen for race gui
    irr_driver->getVideoDriver()->setViewPort(core::recti(0, 0,
        irr_driver->getActualScreenSize().Width,
        irr_driver->getActualScreenSize().Height));

    m_current_screen_size = core::vector2df(
                                    (float)irr_driver->getActualScreenSize().Width,
                                    (float)irr_driver->getActualScreenSize().Height);

    for(unsigned int i=0; i<Camera::getNumCameras(); i++)
    {
        Camera *camera2 = Camera::getCamera(cam);
        std::ostringstream oss2;
        oss2 << "renderPlayerView() for kart " << cam;

        PROFILER_PUSH_CPU_MARKER(oss2.str().c_str(), 0x00, 0x00, (cam+1)*60);
        rg->renderPlayerView(camera2, dt);

        PROFILER_POP_CPU_MARKER();
    }  // for i<getNumKarts

    {
        ScopedGPUTimer Timer(irr_driver->getGPUTimer(Q_GUI));
        PROFILER_PUSH_CPU_MARKER("GUIEngine", 0x75, 0x75, 0x75);
        // Either render the gui, or the global elements of the race gui.
        GUIEngine::render(dt);
        PROFILER_POP_CPU_MARKER();
    }

    // Render the profiler
    if(UserConfigParams::m_profiler_enabled)
    {
        PROFILER_DRAW();
    }

#ifdef DEBUG
    drawDebugMeshes();
#endif



/// ... à là (maj Sami)

for (int cam=0;cam<2;cam++) {
Camera *camera2 = Camera::getCamera(cam);
std::ostringstream oss2;
oss2 << "renderPlayerView() for kart " << cam;

PROFILER_PUSH_CPU_MARKER(oss2.str().c_str(), 0x00, 0x00, (cam+1)*60);
rg->renderPlayerView(camera2, dt);

PROFILER_POP_CPU_MARKER();}*/

    if (UserConfigParams::m_nb_views >1 ) {


    MultiViewShader::getInstance()->use();

    glBindVertexArray(SharedGPUObjects::getUI_VAO());


  //  GLuint texture_cam_id = renderTarget->getTexture();

    GLuint txc_id1 = views_tx_objects[0]->getTexture();
    GLuint txc_id2 = views_tx_objects[1]->getTexture();
    GLuint txc_id3 = views_tx_objects[2]->getTexture();
    GLuint txc_id4 = views_tx_objects[3]->getTexture();
    GLuint txc_id5 = views_tx_objects[4]->getTexture();
    GLuint txc_id6 = views_tx_objects[5]->getTexture();

 MultiViewShader::getInstance()->setTextureUnits(txc_id1,txc_id2,txc_id3,txc_id4);

  /*
   MultiViewShader::getInstance()->setUniforms(...
                    core::vector2df(center_pos_x, center_pos_y),
                    core::vector2df(width, height),
                    core::vector2df(tex_center_pos_x, tex_center_pos_y),
                    core::vector2df(tex_width, tex_height) );
 */

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    } //end if



    PROFILER_PUSH_CPU_MARKER("EndScene", 0x45, 0x75, 0x45);
    irr_driver->getVideoDriver()->endScene();
    PROFILER_POP_CPU_MARKER();

    m_post_processing->update(dt);
    std::cout << irr_driver->getVideoDriver()->getFPS() << "current Framerate\n";
} //render

// ----------------------------------------------------------------------------
std::unique_ptr<RenderTarget> ShaderBasedRenderer::createRenderTarget(const irr::core::dimension2du &dimension,
                                                                      const std::string &name)
{
    return std::unique_ptr<RenderTarget>(new GL3RenderTarget(dimension, name, this));
    //return std::make_unique<GL3RenderTarget>(dimension, name, this); //require C++14
}

// ----------------------------------------------------------------------------
void ShaderBasedRenderer::renderToTexture(GL3RenderTarget *render_target,
                                          irr::scene::ICameraSceneNode* camera,
                                          float dt)
{
    // For render to texture no triple buffering of ubo is used
    SP::sp_cur_player = 0;
    SP::sp_cur_buf_id[0] = 0;
    assert(m_rtts != NULL);

    irr_driver->getSceneManager()->setActiveCamera(camera);
    static_cast<scene::CSceneManager *>(irr_driver->getSceneManager())
        ->OnAnimate(os::Timer::getTime());
    computeMatrixesAndCameras(camera, m_rtts->getWidth(), m_rtts->getHeight());
    if (CVS->isARBUniformBufferObjectUsable())
        uploadLightingData();


    if (CVS->isDeferredEnabled())
    {
        renderSceneDeferred(camera, dt, false, true);
        render_target->setFrameBuffer(m_post_processing
            ->render(camera, false, m_rtts));
    }
    else
    {
        renderScene(camera, dt, false, true);
        render_target->setFrameBuffer(&m_rtts->getFBO(FBO_COLORS));
    }

    // reset
    glViewport(0, 0,
        irr_driver->getActualScreenSize().Width,
        irr_driver->getActualScreenSize().Height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //irr_driver->getSceneManager()->setActiveCamera(NULL);

} //renderToTexture

#endif   // !SERVER_ONLY
