#include <SFML/Graphics.hpp>
#include <stdio.h>

#include "MeshImage.h"
#include "FluidSimulation.h"
#include "VectorField.h"
#include "ToggleButton.h"
#include <string>

using namespace sf;

// handle keyboard events, for now only if simulation should close
void handleKeyboard(RenderWindow &window, Event &event)
{
	if (event.type == sf::Event::KeyPressed)
	{
		switch (event.key.code)
		{
		case sf::Keyboard::Escape:
			window.close();
			break;
		default:
			printf("Unbound key %d.\n", event.key.code);
		}
	}
}

// Handle events, for now only if simulation should close
void handleEvents(RenderWindow &window, Event &event)
{
	if (event.type == sf::Event::Closed)
		window.close();
	handleKeyboard(window, event);
}

int main()
{
	Vector2u simRes(800, 800);																																																												// pixel size of simulation
	Vector2u toolRes(simRes.x, 50);																																																										// pixel size of UI portion
	Vector2u resolution(simRes.x, simRes.y + toolRes.y);																																															// total pixel size of contents
	RenderWindow window(sf::VideoMode(resolution.x, resolution.y), "Fluid Simulation Project by Pontus Asp", sf::Style::Titlebar | sf::Style::Close); // Create a window

	Color bgColor(30, 30, 32); // Our clearing color, and background color for toolbar

	// Initialize our Fluid Sim class with a 200x200 grid resolution, with a 1/8 scale on the
	// vectorfield (every 8th grid will have a vector arrow). Then we set the diffusion and viscosity values.
	FluidSimulation sim(200, .125, 0.00000001f, 0.00000018f);
	sim.setScale(Vector2f(simRes.x, simRes.y)); // Scale up our simulation to fit the screen.

	// load font for buttons
	sf::Font font;
	if (!font.loadFromFile("upheavtt.ttf"))
	{
		printf("Failed to load font!\n");
		return -1;
	}

	int buttons = 0;		 // button counter for easier placement of them
	bool paused = false; // dictates if simulation is paused or not
	bool bExportFoil = false;
	bool bImportFoil = false;
	float marginLeft = 12.5f; // margin of each button
	float bWidth = 145;				// width of each button

	// Create background rectangle behind buttons
	RectangleShape uiBorder;
	float borderThickness = 2.f;
	uiBorder.setSize(Vector2f(toolRes.x - borderThickness * 2, toolRes.y - borderThickness * 2));
	uiBorder.setOutlineColor(Color(100, 100, 100));
	uiBorder.setFillColor(bgColor);
	uiBorder.setOutlineThickness(borderThickness);
	uiBorder.setPosition(borderThickness, simRes.y + borderThickness);

	// Initialize all our buttons, probably pretty self explanatory, check header file to see what each value does.
	ToggleButton vectorFieldToggle(&sim.vectorFieldActive, "vector field", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
																 Color(0, 150, 0), Color(40, 40, 40), Color::White, Color::White, font);

	ToggleButton clearWallsToggle(&sim.shouldClearWalls, "clear walls", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
																Color(40, 40, 40), Color(150, 0, 160), Color::White, Color::White, font);

	ToggleButton resetToggle(&sim.shouldReset, "reset", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
													 Color(40, 40, 40), Color(180, 0, 0), Color::White, Color::White, font);

	ToggleButton exportFoil(&bExportFoil, "Export foil", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
													Color(0, 0, 200), Color(40, 40, 40), Color::White, Color::White, font);

	ToggleButton importFoil(&bImportFoil, "Import foil", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
													Color(0, 0, 200), Color(40, 40, 40), Color::White, Color::White, font);

	bool nextStep = true;
	ToggleButton nextStepToggle(&nextStep, "timestep + 1", Vector2f(marginLeft + (marginLeft + bWidth) * buttons++, 815), Vector2f(bWidth, 20),
															Color(200, 80, 0), Color(200, 80, 0), Color::White, Color::White, font);

	// Our main loop that will power everthing in our simulation and window
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			handleEvents(window, event);
		}

		// Check our buttons if we have clicked on them
		vectorFieldToggle.Update(window);
		exportFoil.Update(window);
		importFoil.Update(window);
		clearWallsToggle.Update(window);
		resetToggle.Update(window);
		if (paused) // Only check for clicks on the stepping button if the game is paused
			nextStepToggle.Update(window);

		if (!paused)
		{
			sim.Step(.1f, 5); // Keep running simulation if not paused
			nextStep = true;
		}
		else if (nextStep)
		{
			sim.Step(.1f, 5); // Will step only once before nextStep is reset and has to be set to true through button press
			nextStep = false;
		}
		if (bExportFoil)
		{
			sim.ExportFoil();
			bExportFoil = false;
		}
		if (bImportFoil)
		{
			sim.LoadFoil();
			bImportFoil = false;
		}
		sim.HandleMouse(window); // Update mouse in simulation (draw walls and velocity/density)
		sim.UpdateImage();			 // Update our MeshImage and VectorField in the simulation to prepare to be drawn to screen

		window.clear(bgColor); // clear screen

		window.draw(sim); // draws our MeshImage and VectorField (which shows density and velocity respectively)

		// Draw our UI background and buttons
		window.draw(uiBorder);
		window.draw(vectorFieldToggle);
		window.draw(exportFoil);
		window.draw(importFoil);
		window.draw(clearWallsToggle);
		window.draw(resetToggle);
		if (paused) // Only draw the stepping button if simulation is paused
			window.draw(nextStepToggle);

		window.display(); // Push everything we have drawn to the buffer to the screen
	}

	return 0;
}
