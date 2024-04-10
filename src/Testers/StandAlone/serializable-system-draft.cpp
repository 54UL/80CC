
// SceneComponent
class someComponent: NodeComponent {
    String field1;

    serializedBuffer Serialize()
    {
        auto componentDef = new componenetDef() // Dymanic message factory instance...
        vector<Serializeble> componentSerializable = {{"field1", SERIALIZABLE_STRING}};
        
        return componentDef.Serialize(componentSerialiable);
    }

    void Deserialize(binaryBuffer bin) 
    {
        
    }
}

class someComponent2: NodeComponent {
    int field2;
    float fieldo3;
}

SceneNode::SerializeNode(){
    // Serialize node info itself then components...
    for(serializeble:NodeComponent->serializebles){
        serializeble->StoreToFile();
    }
}