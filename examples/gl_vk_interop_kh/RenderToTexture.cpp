// Copyright (c) 2011-2021 Sundog Software LLC. All rights reserved worldwide.

#include "RenderToTexture.h"

namespace Sample
{

RenderToTexture::RenderToTexture(int _width, int _height)
    : myWidth(_width), myHeight(_height)
{
    glGenFramebuffers(1, &myFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, myFbo);

    glGenTextures(1, &myColorTexture);
    glBindTexture(GL_TEXTURE_2D, myColorTexture);
    glTexStorage2D(GL_TEXTURE_2D, 9, GL_RGBA8, myWidth, myHeight);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &myDepthTexture);
    glBindTexture(GL_TEXTURE_2D, myDepthTexture);
    glTexStorage2D(GL_TEXTURE_2D, 9, GL_DEPTH_COMPONENT32F, myWidth, myHeight);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, myColorTexture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, myDepthTexture, 0);

    static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, draw_buffers);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
RenderToTexture::~RenderToTexture()
{
    glDeleteFramebuffers(1, &myFbo);
    glDeleteTextures(1, &myColorTexture);
    glDeleteTextures(1, &myDepthTexture);
}

void RenderToTexture::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, myFbo);
    glViewport(0, 0, myWidth, myHeight);
}
void RenderToTexture::unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderToTexture::bindColorTex() const
{
    glBindTexture(GL_TEXTURE_2D, myColorTexture);
}
void RenderToTexture::unbindColorTex() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint RenderToTexture::GetFBO(void) const
{
    return myFbo;
}
}
