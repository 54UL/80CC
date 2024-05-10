
#include <Graphics/Rendering/Entities/Sprite.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ettycc
{
    Sprite::Sprite()
    {
        initializable_ = true;
    }

    Sprite::Sprite(const std::string &spriteFilePath, bool initialize) : spriteFilePath_(spriteFilePath)
    {
        initializable_ = initialize;

        // if null nothing to do (THIS IS USED FOR UNIT TESTING)
        if (spriteFilePath.empty() || !initialize)
        {
            return;
        }
        Init();
    }

    auto Sprite::InitBackend() -> void
    {

        // Quad vertices and texture coordinates
        float vertices[]{
            // Positions         // Texture Coords
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f};

        // Indices to form two triangles for the quad
        unsigned int indices[]{
            0, 1, 2,
            2, 3, 0};

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
        // TODO: THIS OPERATION NEEDS TO BE EAGEAR INITIALIZED INSTEAD OF LAZY INIT...
        auto resources = GetDependency(Resources);
        auto shadersPath = resources->Get("paths","shaders");

        // TODO: Make shader types constants...
        auto vertexShaderSource = LoadShaderFile(shadersPath + shaderBaseName_ + ".vert");
        auto fragmentShaderSource = LoadShaderFile(shadersPath + shaderBaseName_ + ".frag");
        
        std::vector<std::shared_ptr<Shader>> shadersIntances =
            {
                std::make_shared<Shader>(vertexShaderSource.c_str(), GL_VERTEX_SHADER),
                std::make_shared<Shader>(fragmentShaderSource.c_str(), GL_FRAGMENT_SHADER)
            };
        // Shader sources
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
        unsigned char *image = stbi_load(spriteFilePath_.c_str(), &width, &height, &numChannels, 0);

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
    void Sprite::Init() 
    {
        if (!spriteFilePath_.empty() && initializable_)
        {
            spdlog::info("Initializing loaded sprite [{}]", spriteFilePath_);
            InitBackend();
        }
    }

    void Sprite::Pass(const std::shared_ptr<RenderingContext> &ctx, float time)
    {
        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TEXTURE);

        // Use the shader program and draw the quad
        underlyingShader.Bind();

        // Multiply projection * view * model matrix to compute sprite rendering...
        glm::mat4 ProjectionViewMatrix = ctx->Projection * ctx->View * underylingTransform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(underlyingShader.GetProgramId(), "PVM"), 1, GL_FALSE, glm::value_ptr(ProjectionViewMatrix));

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

    std::string Sprite::LoadShaderFile(const std::string &shaderPath)
    {
        std::ifstream file(shaderPath);
        
        if (!file.is_open())
        {
            spdlog::error("Faled to open shader file {}", shaderPath);
            return std::string();
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}