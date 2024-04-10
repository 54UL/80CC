#include <gtest/gtest.h>

#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
using namespace ettycc;


class SerializationTestFixture : public testing::Test
{
protected:
    void SetUp() override
    {
 
    }

    void TearDown() override
    {
     
    }
};

bool compareFloats(double a, double b) {
    double epsilon = 1e-9;
    return std::abs(a - b) < epsilon;
}

TEST_F(SerializationTestFixture, basic_serialization)
{
    const int32_t intTestValue = 54;
    const float   floatTestValue = 0.3232f;
    const const char* stringTestValue = "hello-world =3";
    const glm::vec3 vec3TestValue = glm::vec3(0,1,0);
    const glm::mat4 mat4TestValue = glm::mat4(1);
    
    // Construct an object
    std::vector<Serializable> serializables = 
    {
        Serializable(SerializableType::int32, "intField", intTestValue), 
        Serializable(SerializableType::floating, "floatField", floatTestValue),
        Serializable(SerializableType::string, "stringField", stringTestValue),
        Serializable(SerializableType::vec3, "vector3Field", vec3TestValue),
        Serializable(SerializableType::mat4, "matrixField", mat4TestValue)
    };

    // Serialize to json encoded data... (for the moment...)
    std::vector<uint8_t> data = Serial::Serialize(serializables);
    spdlog::info("json value:{}", data);
    
    // Reads a json string and then hold all the deserialization values into a binary container
    BinaryContainer container = Serial::Deserialize(data);

    // Extract members to execute conversions
    auto intField = container.getInt("intField");
    auto floatField = container.getFloat("floatField");
    auto stringField = container.getString("stringField");
    auto vector3Field = container.getVec3("vector3Field");
    auto matrixField = container.getMat4("matrixField");

    EXPECT_TRUE(intField == intTestValue);
    EXPECT_TRUE(compareFloats(floatField, floatTestValue));
    EXPECT_TRUE(stringField == stringTestValue);
    EXPECT_TRUE(vector3Field == vec3TestValue);
    EXPECT_TRUE(matrixField == mat4TestValue);
}