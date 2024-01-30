#include "Mesh.hpp"
#include "Sprite.hpp"

namespace ettycc
{
    Sprite::Sprite()
    {
        // Quad vertices and texture coordinates
        float vertices[] {
            // Positions         // Texture Coords
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
            0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f
        };

        // Indices to form two triangles for the quad
        unsigned int indices[]  {
            0, 1, 2,
            2, 3, 0
        };

        // Generate and bind a Vertex Array Object (VAO)
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Bind the VAO
        glBindVertexArray(VAO);

        // Bind and set vertex buffer data
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Bind and set element buffer data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Set vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Unbind the VAO (not strictly necessary but advised)
        glBindVertexArray(0);

        LoadShaders();
        LoadTextures();
    }

    Sprite::~Sprite()
    {
        // Clean up
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteTextures(1, &texture);
    }

    void Sprite::LoadShaders()
    {
        std::vector<shared_ptr<Shader>> shadersIntances;

        // Shader sources
        const char *vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec2 aTexCoord;

            out vec2 TexCoord;

            void main()
            {
                gl_Position = vec4(aPos, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const char *fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;

            in vec2 TexCoord;

            uniform sampler2D ourTexture;

            void main()
            {
                FragColor = texture(ourTexture, TexCoord);
            }
        )";

        shadersIntances.emplace_back(std::make_shared<Shader>(vertexShaderSource, GL_VERTEX_SHADER));
        shadersIntances.emplace_back(std::make_shared<Shader>(fragmentShaderSource, GL_FRAGMENT_SHADER));
        underlyingShader.AddShaders(shadersIntances);
        underlyingShader.Create();
    }

    void Sprite::LoadTextures()
    {
        // TODO MOVE BELOW THIS PICES OF CODE...
        // Load and create a texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Set the texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Load an image and generate the texture
        // You can use any image loading library of your choice (e.g., stb_image)
        // For simplicity, let's assume a 1x1 pixel texture with a red color
        unsigned char pixel[] = {255, 0, 0};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel);
        glGenerateMipmap(GL_TEXTURE_2D);

         // Bind the shader program
        underlyingShader.Use();

        // // Set the texture unit in the shader
        glUniform1i(glGetUniformLocation(shader.getProgramId(), "ourTexture"), 0);
    }

    // Renderable
    void Sprite::Pass(const std::shared_ptr<RenderingContext> &ctx)
    {
        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Use the shader program and draw the quad
        underlyingShader.Use();

        // Multiply projection * view * model matrix to compute sprite rendering...
        auto ProjectionViewMatrix = ctx->Projection * ctx->View *  underylingTransform->getMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader.getProgramId(), "PVM"), ProjectionViewMatrix);

        // Bind the rendering mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Unbind the vertex array to avoid problems
        glBindVertexArray(0);
    }
}