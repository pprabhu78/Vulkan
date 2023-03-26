// Copyright (c) 2004-2021  Sundog Software, LLC. All rights reserved worldwide.

#pragma once
#include "GL/glew.h"
#include "RenderToTexture.h"

namespace Sample
{
    //! utilities for rendering a screen aligned class
    //! The 2 smaller views are drawn to a render to texture (rtt)
    //! this class is used to draw that texture on top of the main
    //! view using this screen aligned quad shader.
    class QuadRenderer
    {
    public:
        //! constructor
        QuadRenderer(const std::string& pathToShaders);
        //! constructor
        virtual ~QuadRenderer();
    public:
        virtual void RenderQuad(int x, int y, int width, int height, int mainWindowWidth, int mainWindowHeight);
    protected:
        virtual void CreateQuadProgram(GLuint& quadProgram);
        virtual void CreateQuadVB(GLuint& quadVB);
        virtual void CreateQuadIB(GLuint& quadIB);
    protected:
        //! quad drawing stuff
        unsigned int myQuadVertexBuffer;
        unsigned int myQuadIndexBuffer;
        unsigned int myQuadProgram;

        GLint window_params_index;
        GLint viewport_width_index;
        GLint viewport_height_index;
        GLint tex_index;

        const std::string path_to_shaders;
    };

}

