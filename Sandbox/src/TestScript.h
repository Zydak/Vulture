#pragma once
#include <Vulture.h>

class TestScript : public Vulture::ScriptInterface
{
public:
	TestScript() {}
	~TestScript() {}

	void OnCreate() override
	{
		m_TransformComponent = &GetComponent<Vulture::TransformComponent>();
		m_StartingTranslation = m_TransformComponent->transform.GetTranslation();
		VL_CORE_TRACE("TEST SCRIPT INITIALIZED");
	}

	void OnDestroy() override
	{
		VL_CORE_TRACE("TEST SCRIPT DESTROYED");
	}
	void OnUpdate(double deltaTime) override
	{
		m_TransformComponent = &GetComponent<Vulture::TransformComponent>();
		timer += (float)deltaTime;
		m_TransformComponent->transform.AddRotation({0.0f, 0.0f, 5.0f});
		m_TransformComponent->transform.SetTranslation(glm::vec3(glm::sin(timer) * 12.0f, glm::cos(timer) * 2.0f, 0.0f) + m_StartingTranslation);
	}
private:
	Vulture::TransformComponent* m_TransformComponent;
	glm::vec3 m_StartingTranslation;
	float timer = 0.0f;
};