#pragma once
#include <Vulture.h>

class TestScript : public Vulture::ScriptInterface
{
public:
	TestScript() {}
	~TestScript() {}

	void OnCreate() override
	{
		VL_CORE_TRACE("TEST SCRIPT INITIALIZED");
	}

	void OnDestroy() override
	{
		VL_CORE_TRACE("TEST SCRIPT DESTROYED");
	}
	void OnUpdate(double deltaTime) override
	{
		auto& transformComponent = GetComponent<Vulture::TransformComponent>();
		static float walkSpeed = 0.2f; 
		if (Vulture::Input::IsKeyPressed(VL_KEY_Q))
		{
			transformComponent.transform.AddRotation({ 0.0f, 0.0f, -5.0f });
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_E))
		{
			transformComponent.transform.AddRotation({0.0f, 0.0f, 5.0f});

		}
		
		if (Vulture::Input::IsKeyPressed(VL_KEY_F))
		{
			transformComponent.transform.AddTranslation({-walkSpeed, 0.0f, 0.0f});
			if (!m_Entity.GetScene()->CheckCollisionsWith(m_Entity).empty())
				transformComponent.transform.AddTranslation({ walkSpeed, 0.0f, 0.0f });

		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_H))
		{
			transformComponent.transform.AddTranslation({ walkSpeed, 0.0f, 0.0f });
			if (!m_Entity.GetScene()->CheckCollisionsWith(m_Entity).empty())
				transformComponent.transform.AddTranslation({ -walkSpeed, 0.0f, 0.0f });
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_G))
		{
			transformComponent.transform.AddTranslation({ 0.0f, -walkSpeed, 0.0f });
			auto vec = m_Entity.GetScene()->CheckCollisionsWith(m_Entity);
			if (!vec.empty())
			{
				transformComponent.transform.AddTranslation({ 0.0f, walkSpeed, 0.0f });
			}
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_T))
		{
			transformComponent.transform.AddTranslation({ 0.0f, walkSpeed, 0.0f });
			if (!m_Entity.GetScene()->CheckCollisionsWith(m_Entity).empty())
				transformComponent.transform.AddTranslation({ 0.0f, -walkSpeed, 0.0f });
		}
	
	}
private:

};