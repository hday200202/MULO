#pragma once
#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"

class DecibelComponent : public MULOComponent {
public:
    inline DecibelComponent(){ name = "DecReader" }
    ~DecibelComponent() override{}

    void init() override;
    bool handleEvents() override;
    void update() override;

private:
    Row* decReaderRow = nullptr;
};

void DecibelComponent::init() {

}

bool DecibelComponent::handleEvents() {

}

void DecibelComponent::update() {
    
}