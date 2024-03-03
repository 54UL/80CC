
//SceneComponent
class someComponent: NodeComponent {
    Serializable<string> field1;
}

class someComponent2: NodeComponent {
    Serializable<string> field2;
    Serializable<int>    field3;
}


SceneNode::SerializeNode(){
    // Serialize node info itself then components...
    for(serializeble:NodeComponent->serializebles){
        serializeble->StoreToFile();
    }
}