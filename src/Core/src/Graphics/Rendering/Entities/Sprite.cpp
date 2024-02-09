
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <glm/gtc/type_ptr.hpp> 
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ettycc
{
    Sprite::Sprite(const char * spriteFilePath): spriteFilePath_(spriteFilePath)
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
        glDeleteTextures(1, &TEXTURE);
    }

    void Sprite::LoadShaders()
    {
        std::vector<std::shared_ptr<Shader>> shadersIntances;
        // TODO: FETCH SOURCES FROM FILE...

        // Shader sources
        const char *vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec2 aTexCoord;

            out vec2 TexCoord;
            uniform  mat4 PVM; 

            void main()
            {
                gl_Position = PVM * vec4(aPos, 1);
                // gl_Position =  vec4(aPos, 1);

                TexCoord = aTexCoord;
            }
        )";

        const char *fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;

            in vec2 TexCoord;
            uniform float time;

            uniform sampler2D ourTexture;
 

            void main()
            {
              
                // vec2 yFlipped = vec2(TexCoord.x, 1 - TexCoord.y);
                // FragColor = texture(ourTexture, yFlipped);

                // Get the original texture color
                vec4 originalColor = texture(ourTexture, vec2(TexCoord.x,(1-TexCoord.y)));

                // Add a sine wave effect to the y-coordinate
                float frequency = 5.0; // Adjust the frequency of the sine wave
                float amplitude = 0.1; // Adjust the amplitude of the sine wave
                float wave = amplitude * sin(2.0 * 3.14159 * frequency * TexCoord.x + time) + cos(1 * 3.14159 * frequency * TexCoord.y + time*10);

                // Apply the sine wave effect to the y-coordinate
                vec2 distortedTexCoord = vec2(TexCoord.x, (1-TexCoord.y) + wave);

                // Sample the texture with the distorted texture coordinates
                vec4 distortedColor = texture(ourTexture, distortedTexCoord);

                // Blend the original and distorted colors
                // FragColor = mix(originalColor, distortedColor, 0.5);
                FragColor = distortedColor;
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

        glGenTextures(1, &TEXTURE);
        glBindTexture(GL_TEXTURE_2D, TEXTURE);

        // Set the texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // load the image with stbi_load
        int width, height, numChannels;
        unsigned char* image = stbi_load(spriteFilePath_, &width, &height, &numChannels, 0);

        // Pass the data to the gpu
        if (image)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Free the image data after generating the texture
            stbi_image_free(image);
        }
        else
        {
            // Handle error loading the image
            std::cerr << "Error loading image: " << stbi_failure_reason() << std::endl;
        }

        // Bind the shader program
        underlyingShader.Bind();

        // Set the texture unit in the shader
        glUniform1i(glGetUniformLocation(underlyingShader.GetProgramId(), "ourTexture"), 0);

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Renderable
    void Sprite::Pass(const std::shared_ptr<RenderingContext> &ctx, float time)
    {
        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TEXTURE);

        // Use the shader program and draw the quad
        underlyingShader.Bind();

        // Multiply projection * view * model matrix to compute sprite rendering...
        glm::mat4 ProjectionViewMatrix = ctx->Projection * ctx->View *  underylingTransform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(underlyingShader.GetProgramId(), "PVM"),1, GL_FALSE,  glm::value_ptr(ProjectionViewMatrix));

        // Time is passed to the shader program
        glUniform1f(glGetUniformLocation(underlyingShader.GetProgramId(), "time"), time);

        // Bind the rendering mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Unbind the vertex array to avoid problems
        underlyingShader.Unbind();
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}