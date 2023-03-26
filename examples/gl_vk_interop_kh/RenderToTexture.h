// Copyright (c) 2004-2021  Sundog Software, LLC. All rights reserved worldwide.

#pragma once

#include "GL/glew.h"

namespace Sample
{
    class RenderToTexture
    {
    public:
        //! constructor
        RenderToTexture(int width, int height);

        //! destructor
        ~RenderToTexture();

    public:
        //! bind the rtt
        virtual void bind() const;

        //! unbind the rtt
        virtual void unbind() const;

        //! bind the color texture
        virtual void bindColorTex() const;

        //! unbind the color texture
        virtual void unbindColorTex() const;

        //! get the underlying fbo
        virtual GLuint GetFBO(void) const;
    private:
        GLuint myFbo;
        GLuint myColorTexture;
        GLuint myDepthTexture;

        const int myWidth;
        const int myHeight;
    };

}

