#include <iostream>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <google/protobuf/message.h>

// Base class for node components
class NodeComponentBase {
public:
    virtual ~NodeComponentBase() = default;
    virtual void Serialize(google::protobuf::Message& proto) const = 0;
    virtual void Deserialize(const google::protobuf::Message& proto) = 0;
};

// Specific node component implementations
class StringNodeComponent : public NodeComponentBase {
public:
    std::string data;

    void Serialize(google::protobuf::Message& proto) const override {
        // Serialize data to proto
    }

    void Deserialize(const google::protobuf::Message& proto) override {
        // Deserialize data from proto
    }
};

class IntNodeComponent : public NodeComponentBase {
public:
    int data;

    void Serialize(google::protobuf::Message& proto) const override {
        // Serialize data to proto
    }

    void Deserialize(const google::protobuf::Message& proto) override {
        // Deserialize data from proto
    }
};

// Serialization/Deserialization module
class NodeComponentSerializer {
public:
    template <typename T>
    static std::vector<uint8_t> SerializeComponent(const T& component) {
        // Create a protobuf message for serialization
        // Serialize the component to the protobuf message
        // Convert the serialized protobuf message to bytes and return
    }

    template <typename T>
    static void DeserializeComponent(const std::vector<uint8_t>& data, T& component) {
        // Convert the bytes to a protobuf message
        // Deserialize the protobuf message to the component
    }
};

// Main Code
int main() {
    // Example usage
    StringNodeComponent stringComponent;
    stringComponent.data = "Hello, World!";

    std::vector<uint8_t> serializedStringComponent = NodeComponentSerializer::SerializeComponent(stringComponent);

    StringNodeComponent deserializedStringComponent;
    NodeComponentSerializer::DeserializeComponent(serializedStringComponent, deserializedStringComponent);

    // Similarly, you can use IntNodeComponent or any other specific node component type

    return 0;
}
