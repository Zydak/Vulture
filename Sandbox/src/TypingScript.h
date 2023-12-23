#pragma once

#include <Vulture.h>

class TypingScript : public Vulture::ScriptInterface
{
public:
	TypingScript() = default;
	void OnCreate() override
	{

	}

	void OnDestroy() override
	{

	}

	void OnUpdate(double deltaTime) override
	{
		auto& textComponent = GetComponent<Vulture::TextComponent>();

		textTimer += (float)deltaTime;
		promptTimer += (float)deltaTime;

		if (textTimer >= textFrequency)
		{
			textTimer -= textFrequency;
			if (!remove)
			{
				writing = true;
				promptChar = "|";
				displayedText += strings[j][i];
				textComponent.ChangeText(displayedText + promptChar);

				if (strings[j][i] == '.' || strings[j][i] == '?' || strings[j][i] == '!')
				{
					textTimer -= 0.5f; // Pause at certian signs;
				}
				i++;
				if (i >= (int)strings[j].size()) // if all characters are written
				{
					remove = true; // start removing
					i = 0;
					textTimer -= 3.0f; // wait 3s
					writing = false;
					promptChar = "";
				}
			}
			else
			{
				writing = true;
				promptChar = "|";
				displayedText.pop_back();
				textComponent.ChangeText(displayedText + promptChar);
				i++;
				textTimer += 0.02f;
				if (i >= (int)strings[j].size()) // if all characters are removed
				{
					remove = false; // start writing again
					i = 0;
					textTimer -= 3.0f; // wait 3s
					writing = false;
					promptChar = "";
					j++;
					if (j >= (int)strings.size())
					{
						j = 0;
					}
				}
			}
		}
		if (promptTimer >= promptFrequency)
		{
			promptTimer -= promptFrequency;
			if (!writing) // disable blinking during writing
			{
				promptChar = promptChar == "|" ? "" : "|";
				textComponent.ChangeText(displayedText + promptChar);
			}
		}
	}

	std::vector<std::string> strings;
private:
	std::string displayedText = "";
	std::string promptChar = "|";
	int i = 0;
	int j = 0;
	float textTimer = 0.0f;
	float promptTimer = 0.0f;
	float textFrequency = 0.1f; // frequency at which characters will appear;
	float promptFrequency = 0.5f; // frequency at which prompt will blink
	bool remove = false; // whether to write on remove characters;
	bool writing = true;
};