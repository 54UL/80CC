#ifndef BINARY_CONTAINER_HPP
#define BINARY_CONTAINER_HPP

#include <unordered_map>
#include <string>
#include <glm/glm.hpp>

namespace ettycc
{
    class BinaryContainer
    {
    public:
        std::unordered_map<std::string, std::string> values;

        int32_t getInt(const std::string &name)
        {
            return std::stoi(values[name]);
        }

        float getFloat(const std::string &name)
        {
            return std::stof(values[name]);
        }

        std::string getString(const std::string &name)
        {
            return values[name];
        }

        glm::vec3 getVec3(const std::string &name)
        {
            glm::vec3 vec;
            std::sscanf(values[name].c_str(), "(%f, %f, %f)", &vec.x, &vec.y, &vec.z);
            return vec;
        }

        glm::mat4 getMat4(const std::string &name)
        {
            glm::mat4 mat;
            // TODO: tf is this???  improve!!!
            std::sscanf(values[name].c_str(), "[%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
                        &mat[0][0], &mat[0][1], &mat[0][2], &mat[0][3],
                        &mat[1][0], &mat[1][1], &mat[1][2], &mat[1][3],
                        &mat[2][0], &mat[2][1], &mat[2][2], &mat[2][3],
                        &mat[3][0], &mat[3][1], &mat[3][2], &mat[3][3]);
            return mat;
        }
    };
} // namespace ettycc

#endif