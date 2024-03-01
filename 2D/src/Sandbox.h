#pragma once
#include "pch.h"
#include <Vulture.h>
#include "SceneRenderer.h"

class Sandbox : public Vulture::Application
{
public:
	Sandbox(Vulture::ApplicationInfo appInfo, float width, float height);
	~Sandbox();

	void OnUpdate(double deltaTime) override;
	void Destroy() override;
	void InitScripts();
	void UpdateScripts(double deltaTime);
	void DestroyScripts();
private:
	void Init();

	Vulture::Scene m_Scene;

	Vulture::Scope<SceneRenderer> m_SceneRenderer;
};