#pragma warning(disable : 4996)

#define EIGEN_DONT_PARALLELIZE


#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <Eigen/Dense>
#define _USE_MATH_DEFINES
#include <math.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <omp.h>




std::vector<float> toScreenCoords(std::vector<float> axesCoordinates, std::vector<int> windowDimensions);
std::vector<float> rotate3D(std::vector<float> coordinates, float theta, float phi, float psi);


int main()
{
	
	while (true)
	{
		std::string userInputString = "";
		
		// Requests model files to open from the user until valid files are provided and user types 'end'.		
		std::vector<std::string> models;
		std::ifstream modelFile;
		while (true)
		{
			std::cout << "\nPlease enter .dae file name: ";
			std::cin >> userInputString;
			if (userInputString == "end")
			{
				break;
			}

			else
			{
				modelFile.open(userInputString + ".dae");
				if (!modelFile)
				{
					std::cout << "\nFile unable to be found with name \"" << userInputString << ".dae\".\n";
				}
				else
				{
					models.push_back(userInputString);
					modelFile.close();
				}
			}
		}

		


		int FPS = 60;
		int baseFrameTime = 1000000 / FPS;

		std::vector<int> windowDim = { 1000, 800 };

		sf::RenderWindow window(sf::VideoMode(windowDim[0], windowDim[1]), "Title");
		sf::Clock clock;

		sf::Color BLACK(0, 0, 0);
		sf::Color WHITE(255, 255, 255);
		sf::Color RED(255, 0, 0);
		sf::Color GREEN(0, 255, 0);
		sf::Color BLUE(0, 0, 255);

		sf::Font* font = new sf::Font;

		if (!font->loadFromFile("fonts/PressStart2P-Regular.ttf"))
		{
			// error...
		}

		std::vector<sf::Text*> texts;

		sf::Text* angleText = new sf::Text;
		sf::Text* statsText = new sf::Text;
		angleText->setPosition(sf::Vector2f(10, 10));
		statsText->setPosition(sf::Vector2f(10, windowDim[1] - 50));
		statsText->setCharacterSize(statsText->getCharacterSize() / 2);
		texts.push_back(angleText);
		texts.push_back(statsText);


		sf::Text* xAxisLabel = new sf::Text;
		xAxisLabel->setString("x");
		texts.push_back(xAxisLabel);
		sf::Text* yAxisLabel = new sf::Text;
		yAxisLabel->setString("y");
		texts.push_back(yAxisLabel);
		sf::Text* zAxisLabel = new sf::Text;
		zAxisLabel->setString("z");
		texts.push_back(zAxisLabel);

		/*
		sf::Text* renderOrder = new sf::Text;
		renderOrder->setString("1");
		texts.push_back(renderOrder);
		*/

		sf::Text* selectedMeshText = new sf::Text;
		selectedMeshText->setString("Selected Mesh: ");
		selectedMeshText->setPosition(sf::Vector2f(10, 30));
		texts.push_back(selectedMeshText);


		for (int i = 0; i < texts.size(); i++)
		{
			texts[i]->setFont(*font);
			texts[i]->setCharacterSize(15);
			texts[i]->setFillColor(WHITE);
		}


		std::vector<float> points = {};
		std::vector<float> originOld = { 0, 0, 0 };
		std::vector<float> xAxisOld = { 300, 0, 0 };
		std::vector<float> yAxisOld = { 0, 300, 0 };
		std::vector<float> zAxisOld = { 0, 0, 300 };


		std::vector<std::vector<float>> localRotations = {};
		std::vector<std::vector<float>> globalTranslations = {};

		std::vector<std::vector<float>> localRotationsDefault = {};
		std::vector<std::vector<float>> globalTranslationsDefault = {};

		int selectedMeshIndex = 0;

		std::vector<std::string> vertexStringList;
		// List of meshes' vertex coord lists. List(Mesh(Vertex(Coords)))
		std::vector<std::vector<std::vector<float>>> vertexLists;
		std::vector<std::vector<std::vector<float>>> vertexListsCopy;
		// List of meshes' tri' vertices' index lists. List(Mesh(Tri(Vertex(Index, Normal, TextureCoor, MeshIndex))))
		std::vector<std::vector<std::vector<std::vector<int>>>> triLists;
		// List of meshes' normal coord lists. ? List(Mesh(Normal(Coords)))
		std::vector<std::vector<std::vector<float>>> normalLists;

		/*
		Extracts vertex data, tri data and normals data.
		*/
		int vertexCount = 0;
		int triCount = 0;
		int normalCount = 0;

		int meshTotal = 0;

		for (int j = 0; j < models.size(); j++)
		{
			// Reads model file line by line into modelLineStrings vector then closes file.
			modelFile.open(models[j] + ".dae");

			std::vector<std::string> modelLineStrings;
			std::string curLine;

			while (std::getline(modelFile, curLine))
			{
				modelLineStrings.push_back(curLine);
			}

			modelFile.close();

			for (int i = 0; i < modelLineStrings.size(); i++)
			{
				if (boost::algorithm::contains(modelLineStrings[i], "float_array") &&
					boost::algorithm::contains(modelLineStrings[i], "mesh-positions-array"))
				{
					/*
					Eliminates whitespace and edge tags from vert data.
					*/
					std::string trimmedLine = boost::algorithm::trim_copy(modelLineStrings[i]);
					// Finds beginning index of data.
					int dataIndexBegin;
					for (int c = 0; c < trimmedLine.size(); c++)
					{
						if (trimmedLine[c] == '>')
						{
							dataIndexBegin = c + 1;
							break;
						}
					}
					// Finds length of data.
					int dataLength = 0;
					for (int c = dataIndexBegin; c < trimmedLine.size(); c++)
					{
						if (trimmedLine[c] == '<')
						{
							break;
						}
						dataLength++;
					}
					trimmedLine = trimmedLine.substr(dataIndexBegin, dataLength);
					vertexStringList.push_back(trimmedLine);
					std::vector<std::string> lineData;
					boost::algorithm::split(lineData, trimmedLine, boost::is_space());
					std::vector<std::vector<float>> lineFloatData;
					for (int j = 0; j < (lineData.size() / 3); j++)
					{
						std::vector<float> vertexCoords;
						vertexCoords.push_back(std::stof(lineData[(3 * j)]));
						vertexCoords.push_back(std::stof(lineData[(3 * j) + 1]));
						vertexCoords.push_back(std::stof(lineData[(3 * j) + 2]));

						lineFloatData.push_back(vertexCoords);
						vertexCount++;
					}
					vertexLists.push_back(lineFloatData);
				}
				vertexListsCopy = vertexLists;

				if (boost::algorithm::contains(modelLineStrings[i], "<p>"))
				{
					// TODO: FIX OFFSET FOR DIFFERENT DATA NUMBERS
					// Eliminates whitespace and edge tags from tri, normals data.
					std::string trimmedLine = boost::algorithm::trim_copy(modelLineStrings[i]);
					trimmedLine = trimmedLine.substr(3, trimmedLine.size() - 5);
					// Splits data by whitespace
					std::vector<std::string> lineData;
					boost::algorithm::split(lineData, trimmedLine, boost::is_space());
					// Converts string data into ints, step 3.

					int coordTypes = 3;

					triCount += lineData.size() / (coordTypes * 3);
					std::vector<std::vector<std::vector<int>>> triList;
					for (int j = 0; j < (lineData.size() / (coordTypes * 3)); j++)
					{
						std::vector<std::vector<int>> tri;

						// Fills tri vector with corresponding indices.
						tri.push_back({ std::stoi(lineData[(3 * coordTypes) * j])					, std::stoi(lineData[(3 * coordTypes) * j + 1]),					std::stoi(lineData[(3 * coordTypes) * j + 2]), meshTotal });
						tri.push_back({ std::stoi(lineData[(3 * coordTypes) * j + coordTypes])		, std::stoi(lineData[(3 * coordTypes) * j + coordTypes + 1]),		std::stoi(lineData[(3 * coordTypes) * j + coordTypes + 2]), meshTotal });
						tri.push_back({ std::stoi(lineData[(3 * coordTypes) * j + (2 * coordTypes)]), std::stoi(lineData[(3 * coordTypes) * j + (2 * coordTypes) + 1]), std::stoi(lineData[(3 * coordTypes) * j + (2 * coordTypes) + 2]), meshTotal });

						triList.push_back(tri);
					}
					triLists.push_back(triList);
					
					meshTotal++;
					localRotations.push_back({ 0, 0, 0 });
					globalTranslations.push_back({ 0, 0, 0 });
				}

				if (boost::algorithm::contains(modelLineStrings[i], "float_array") &&
					boost::algorithm::contains(modelLineStrings[i], "mesh-normals-array"))
				{
					/*
					Eliminates whitespace and edge tags from norm data.
					*/
					std::string trimmedLine = boost::algorithm::trim_copy(modelLineStrings[i]);
					// Finds beginning index of data.
					int dataIndexBegin;
					for (int c = 0; c < trimmedLine.size(); c++)
					{
						if (trimmedLine[c] == '>')
						{
							dataIndexBegin = c + 1;
							break;
						}
					}
					// Finds length of data.
					int dataLength = 0;
					for (int c = dataIndexBegin; c < trimmedLine.size(); c++)
					{
						if (trimmedLine[c] == '<')
						{
							break;
						}
						dataLength++;
					}
					trimmedLine = trimmedLine.substr(dataIndexBegin, dataLength);
					std::vector<std::string> lineData;
					boost::algorithm::split(lineData, trimmedLine, boost::is_space());
					std::vector<std::vector<float>> lineFloatData;
					for (int j = 0; j < (lineData.size() / 3); j++)
					{
						std::vector<float> normalCoords;
						normalCoords.push_back(std::stof(lineData[(3 * j)]));
						normalCoords.push_back(std::stof(lineData[(3 * j) + 1]));
						normalCoords.push_back(std::stof(lineData[(3 * j) + 2]));

						lineFloatData.push_back(normalCoords);
						normalCount++;
					}
					normalLists.push_back(lineFloatData);
				}
			}
		}
		// Finds default scale by looking at max distance vertex.
		float maxDistance = 0;
		for (int i = 0; i < vertexLists.size(); i++)
		{
			for (int j = 0; j < vertexLists[i].size(); j++)
			{
				float vertexDistance = std::sqrtf(pow(vertexLists[i][j][0], 2) + pow(vertexLists[i][j][1], 2) + pow(vertexLists[i][j][2], 2));
				if (vertexDistance > maxDistance)
				{
					maxDistance = vertexDistance;
				}
			}
		}
		int initialScale = 400 * (1.0f / maxDistance);
		int scale = initialScale;

		sf::RectangleShape frameWipe(sf::Vector2f(windowDim[0], windowDim[1]));
		frameWipe.setFillColor(BLACK);
		sf::Event event;
		float lastTime = 0;
		int currentFps = 0;

		
		std::vector<float> globalRotations = { 7 * M_PI/4, M_PI/9, 0.f };
		std::vector<float> globalRotationsDefault = globalRotations;

		globalTranslationsDefault = globalTranslations;
		localRotationsDefault = localRotations;

		bool leftMouseHeld = false;
		bool rightMouseHeld = false;
		sf::Vector2i mousePosOld = sf::Mouse::getPosition();

		bool capFrameRate = true;

		// Vertex, face, edges and axes toggle booleans
		bool vEnabled = true;
		bool fEnabled = false;
		bool eEnabled = false;
		bool axesEnabled = true;

		bool rotationEnabled = false;

		bool closeModel= false;
		while (!closeModel)
		{
			clock.restart();

			sf::Vector2i mousePos = sf::Mouse::getPosition();

			while (window.pollEvent(event))
			{
				if (event.type == sf::Event::Closed)
				{
					window.close();
					closeModel = true;
				}

				else if (event.type == sf::Event::MouseButtonPressed)
				{
					if (event.mouseButton.button == sf::Mouse::Left)
					{
						leftMouseHeld = true;
					}
					if (event.mouseButton.button == sf::Mouse::Right)
					{
						rightMouseHeld = true;
					}
				}
				else if (event.type == sf::Event::MouseWheelScrolled)
				{
					float newScale = scale + initialScale * 0.1 * event.mouseWheelScroll.delta;
					if (newScale > 0.01 * initialScale && newScale < 100 * initialScale)
					{
						scale = newScale;
					}
				}

				else if (event.type == sf::Event::MouseButtonReleased)
				{
					if (event.mouseButton.button == sf::Mouse::Left)
					{
						leftMouseHeld = false;
					}
					if (event.mouseButton.button == sf::Mouse::Right)
					{
						rightMouseHeld = false;
					}
				}

				else if (event.type == sf::Event::KeyPressed)
				{
					if (event.key.code == sf::Keyboard::Escape)
					{
						closeModel = true;
					}

					// Movement controls
					if (event.key.code == sf::Keyboard::Left)
					{						
						globalTranslations[selectedMeshIndex][0] -= 0.01;
					}
					else if (event.key.code == sf::Keyboard::Right)
					{
						globalTranslations[selectedMeshIndex][0] += 0.01;
					}
					else if (event.key.code == sf::Keyboard::Up)
					{
						globalTranslations[selectedMeshIndex][1] += 0.01;
					}
					else if (event.key.code == sf::Keyboard::Down)
					{
						globalTranslations[selectedMeshIndex][1] -= 0.01;
					}
					else if (event.key.code == sf::Keyboard::Z)
					{
						globalTranslations[selectedMeshIndex][2] += 0.01;
					}
					else if (event.key.code == sf::Keyboard::X)
					{
						globalTranslations[selectedMeshIndex][2] -= 0.01;
					}

					else if (event.key.code == sf::Keyboard::LBracket)
					{
						localRotations[selectedMeshIndex][0] -= M_PI / 8;
						if (localRotations[selectedMeshIndex][0] < 0)
						{
							localRotations[selectedMeshIndex][0] += 2 * M_PI;
						}
					}
					else if (event.key.code == sf::Keyboard::RBracket)
					{
						localRotations[selectedMeshIndex][0] += M_PI / 8;
						if (localRotations[selectedMeshIndex][0] > 2 * M_PI)
						{
							localRotations[selectedMeshIndex][0] -= 2 * M_PI;
						}
					}

					// Other controls
					if (event.key.code == sf::Keyboard::R)
					{
						globalRotations = globalRotationsDefault;

						scale = initialScale;

						globalTranslations = globalTranslationsDefault;
						localRotations = localRotationsDefault;

					}
					if (event.key.code == sf::Keyboard::S)
					{
						rotationEnabled = not rotationEnabled;
						if (rotationEnabled)
						{
							std::cout << "Rotation Enabled\n";
						}
						else
						{
							std::cout << "Rotation Disabled\n";

						}
					}
					if (event.key.code == sf::Keyboard::C)
					{
						capFrameRate = not capFrameRate;
					}

					if (event.key.code == sf::Keyboard::Tab)
					{
						selectedMeshIndex++;
						if (selectedMeshIndex >= meshTotal)
						{
							selectedMeshIndex = 0;
						}
					}

					if (event.key.code == sf::Keyboard::V)
					{
						vEnabled = not vEnabled;
					}
					if (event.key.code == sf::Keyboard::F)
					{
						fEnabled = not fEnabled;
					}
					if (event.key.code == sf::Keyboard::E)
					{
						eEnabled = not eEnabled;
					}
					if (event.key.code == sf::Keyboard::U)
					{
						axesEnabled = not axesEnabled;
					}
				}
			}

			if (leftMouseHeld)
			{
				float xShift = mousePos.x - mousePosOld.x;
				float yShift = mousePos.y - mousePosOld.y;

				
				globalRotations[0] += xShift / 100;
				globalRotations[1] += yShift / 100;

				if (globalRotations[0] > 2 * M_PI)
					globalRotations[0] -= 2 * M_PI;
				if (globalRotations[1] > 2 * M_PI)
					globalRotations[1] -= 2 * M_PI;

				if (globalRotations[0] < 0)
					globalRotations[0] += 2 * M_PI;
				if (globalRotations[1] < 0)
					globalRotations[1] += 2 * M_PI;
			}
			if (rightMouseHeld)
			{
				float xShift = mousePos.x - mousePosOld.x;
				globalRotations[2] += xShift / 100;

				if (globalRotations[2] > 2 * M_PI)
					globalRotations[2] -= 2 * M_PI;
				if (globalRotations[2] < 0)
					globalRotations[2] += 2 * M_PI;

			}

			if (rotationEnabled)
			{
				globalRotations[0] += 0.01;
				if (globalRotations[0] > 2 * M_PI)
					globalRotations[0] -= 2 * M_PI;
			}

			mousePosOld = mousePos;

			std::vector<float> origin = rotate3D(originOld, globalRotations[0], globalRotations[1], globalRotations[2]);
			std::vector<float> xAxisEnd = rotate3D(xAxisOld, globalRotations[0], globalRotations[1], globalRotations[2]);
			std::vector<float> yAxisEnd = rotate3D(yAxisOld, globalRotations[0], globalRotations[1], globalRotations[2]);
			std::vector<float> zAxisEnd = rotate3D(zAxisOld, globalRotations[0], globalRotations[1], globalRotations[2]);

			/*
			// Frame drawing
			*/
			window.draw(frameWipe);

			int meshNumber = vertexLists.size();

			// Draws vertices
			sf::VertexArray vertices(sf::Points, vertexCount);

			int vertexCounter = 0; // Tracks mesh vertices.
			for (int i = 0; i < meshNumber; i++)
			{
#pragma omp parallel for
				// Translation and converts mesh coordinates into screen space coordinates
				for (int j = 0; j < vertexLists[i].size(); j++)
				{
					// Applies scale and local
					std::vector<float> newCoordinates = rotate3D({ (vertexLists[i][j][0]) * scale,
																   (vertexLists[i][j][1]) * scale,
																   (vertexLists[i][j][2]) * scale },
																	localRotations[i][0], localRotations[i][1], localRotations[i][2]);
					// Applies global
					newCoordinates = rotate3D({ (newCoordinates[0] + globalTranslations[i][0] * scale),
												(newCoordinates[1] + globalTranslations[i][1] * scale) ,
												(newCoordinates[2] + globalTranslations[i][2] * scale)},
												 globalRotations[0], globalRotations[1], globalRotations[2]);

					std::vector<float> newCoordinatesScreen = toScreenCoords(newCoordinates, windowDim);
					vertexListsCopy[i][j][0] = newCoordinatesScreen[0];
					vertexListsCopy[i][j][1] = newCoordinatesScreen[1];
					vertexListsCopy[i][j][2] = newCoordinates[2];

					sf::Vertex vertex(sf::Vector2f(newCoordinatesScreen[0], newCoordinatesScreen[1]), sf::Color(255, 255, 255 - i * 255 / meshNumber));
					
					vertices[vertexCounter + j] = vertex;
				}
				vertexCounter += vertexLists[i].size();
			}
			if (vEnabled)
			{
				window.draw(vertices);
			}

			// Draws faces
			int faceDraws = 0;
			
			// Labels last drawn face.
			
			//renderOrder->setPosition(sf::Vector2f(-100, -100));

			std::vector<std::vector<std::vector<int>>> triListRender;
			
			if (fEnabled)
			{
				//SLOW
				
				// Adds each face to a new tri render list
				for (int i = 0; i < meshNumber; i++)
				{
//#pragma omp parallel for
					for (int j = 0; j < triLists[i].size(); j++)
					{	
						// SLOW
						// Backface culling						
						std::vector<float> rotatedNormals = rotate3D(normalLists[i][triLists[i][j][0][1]], localRotations[i][0], localRotations[i][1], localRotations[i][2]);
						// Normal z
						float zDirection = rotate3D({ rotatedNormals[0], rotatedNormals[1], rotatedNormals[2] }, globalRotations[0], globalRotations[1], globalRotations[2])[2];

						if (zDirection > 0)
						{					
							// Sorts tri vertex order by vertex z.						
							std::sort(triLists[i][j].begin(), triLists[i][j].end(), [&](auto& a, auto& b) {
								return vertexListsCopy[i][a[0]][2] > vertexListsCopy[i][b[0]][2];
								});			
							
							triListRender.push_back(triLists[i][j]);
							faceDraws++;
						}		

					}
				}
				
				

				std::sort(triListRender.begin(), triListRender.end(), [&](auto& a, auto& b) {
					return vertexListsCopy[a[0][3]][a[0][0]][2] < vertexListsCopy[b[0][3]][b[0][0]][2];
					});
				

				std::vector<float> renderFirst1 = vertexListsCopy[triListRender[triListRender.size() - 1][0][3]][triListRender[triListRender.size() - 1][0][0]];
				std::vector<float> renderFirst2 = vertexListsCopy[triListRender[triListRender.size() - 1][0][3]][triListRender[triListRender.size() - 1][1][0]];
				std::vector<float> renderFirst3 = vertexListsCopy[triListRender[triListRender.size() - 1][0][3]][triListRender[triListRender.size() - 1][2][0]];

				//renderOrder->setPosition(sf::Vector2f((1.f/3) * (renderFirst1[0] + renderFirst2[0] + renderFirst3[0]) , (1.f / 3) * (renderFirst1[1] + renderFirst2[1] + renderFirst3[1])));

				sf::VertexArray triangles(sf::Triangles, triListRender.size() * 3);

#pragma omp parallel for
				for (int i = 0; i < triListRender.size(); i++)
				{
					sf::VertexArray triangle(sf::Triangles, 3);

					int meshIndex = triListRender[i][0][3];

					triangle[0].position = sf::Vector2f(vertexListsCopy[triListRender[i][0][3]][triListRender[i][0][0]][0], vertexListsCopy[triListRender[i][0][3]][triListRender[i][0][0]][1]);
					triangle[1].position = sf::Vector2f(vertexListsCopy[triListRender[i][0][3]][triListRender[i][1][0]][0], vertexListsCopy[triListRender[i][0][3]][triListRender[i][1][0]][1]);
					triangle[2].position = sf::Vector2f(vertexListsCopy[triListRender[i][0][3]][triListRender[i][2][0]][0], vertexListsCopy[triListRender[i][0][3]][triListRender[i][2][0]][1]);

					sf::Color gradientPaint;

					float dotProduct = abs(rotate3D(rotate3D(normalLists[triListRender[i][0][3]][triListRender[i][0][1]], localRotations[meshIndex][0], localRotations[meshIndex][1], localRotations[meshIndex][2]), globalRotations[0], globalRotations[1], globalRotations[2])[2]);

					
					// If mesh is selected, paints yellow not green
					if (meshIndex == selectedMeshIndex)
					{
						gradientPaint = sf::Color(15 + 240 * abs(rotate3D(rotate3D(normalLists[triListRender[i][0][3]][triListRender[i][0][1]], localRotations[meshIndex][0], localRotations[meshIndex][1], localRotations[meshIndex][2]), globalRotations[0], globalRotations[1], globalRotations[2])[2]), 15 + 240 * abs(rotate3D(rotate3D(normalLists[triListRender[i][0][3]][triListRender[i][0][1]], localRotations[meshIndex][0], localRotations[meshIndex][1], localRotations[meshIndex][2]), globalRotations[0], globalRotations[1], globalRotations[2])[2]), 0);
					}

					else
					{
						gradientPaint = sf::Color(0, 15 + 240 * abs(rotate3D(rotate3D(normalLists[triListRender[i][0][3]][triListRender[i][0][1]], localRotations[meshIndex][0], localRotations[meshIndex][1], localRotations[meshIndex][2]), globalRotations[0], globalRotations[1], globalRotations[2])[2]), 0);
					}
					

					//gradientPaint = sf::Color(0, 15 + pow(220, dotProduct), 0);


					triangle[0].color = gradientPaint;
					triangle[1].color = gradientPaint;
					triangle[2].color = gradientPaint;
						
						
					triangles[i*3] = triangle[0];
					triangles[i*3+1] = triangle[1];
					triangles[i*3+2] = triangle[2];
				}
				
				window.draw(triangles);
				
			}
			
			int edgesDrawn = 0;
			if (eEnabled)
			{
				// Draws edges
				sf::VertexArray lines(sf::Lines);
				for (int i = 0; i < meshNumber; i++)
				{
					for (int j = 0; j < triLists[i].size(); j++)
					{
						for (int k = 0; k < 3; k++)
						{
							lines.append(sf::Vertex(sf::Vector2f(vertexListsCopy[i][triLists[i][j][k][0]][0], vertexListsCopy[i][triLists[i][j][k][0]][1])));
							lines.append(sf::Vertex(sf::Vector2f(vertexListsCopy[i][triLists[i][j][(k+1)%3][0]][0], vertexListsCopy[i][triLists[i][j][(k+1)%3][0]][1])));
							edgesDrawn++;
						}
					}
				}
				
				window.draw(lines);
			}

			// Axes
			std::vector<float> originScreen = toScreenCoords(origin, windowDim);

			std::vector<float> xAxisScreen = toScreenCoords({ xAxisEnd[0] * scale/100, xAxisEnd[1] * scale/100}, windowDim);
			std::vector<float> yAxisScreen = toScreenCoords({ yAxisEnd[0] * scale/100, yAxisEnd[1] * scale/100 }, windowDim);
			std::vector<float> zAxisScreen = toScreenCoords({ zAxisEnd[0] * scale/100, zAxisEnd[1] * scale/100 }, windowDim);

			sf::VertexArray lines(sf::Lines, 6);
			// x-Axis
			sf::VertexArray xLine(sf::Lines, 2);
			xLine[0].position = sf::Vector2f(originScreen[0], originScreen[1]);
			xLine[0].color = RED;
			xLine[1].position = sf::Vector2f(xAxisScreen[0], xAxisScreen[1]);
			xAxisLabel->setPosition(sf::Vector2f(xAxisScreen[0], xAxisScreen[1]));
			xLine[1].color = RED;
			// y-Axis
			sf::VertexArray yLine(sf::Lines, 2);
			yLine[0].position = sf::Vector2f(originScreen[0], originScreen[1]);
			yLine[0].color = GREEN;
			yLine[1].position = sf::Vector2f(yAxisScreen[0], yAxisScreen[1]);
			yAxisLabel->setPosition(sf::Vector2f(yAxisScreen[0], yAxisScreen[1]));
			yLine[1].color = GREEN;
			// z-Axis
			sf::VertexArray zLine(sf::Lines, 2);
			zLine[0].position = sf::Vector2f(originScreen[0], originScreen[1]);
			zLine[0].color = BLUE;
			zLine[1].position = sf::Vector2f(zAxisScreen[0], zAxisScreen[1]);
			zAxisLabel->setPosition(sf::Vector2f(zAxisScreen[0], zAxisScreen[1]));
			zLine[1].color = BLUE;

			if (axesEnabled)
			{
				xAxisLabel->setString("x");
				yAxisLabel->setString("y");
				zAxisLabel->setString("z");

				window.draw(yLine);
				if (zAxisEnd[2] > xAxisEnd[2])
				{
					window.draw(zLine);
					window.draw(xLine);
				}
				else
				{
					window.draw(xLine);
					window.draw(zLine);
				}
			}

			else
			{
				xAxisLabel->setString("");
				yAxisLabel->setString("");
				zAxisLabel->setString("");
			}
			// Texts
			angleText->setString("Theta: " + std::to_string(int(globalRotations[0] * 180 / M_PI)) + "°, Phi: " + std::to_string(int(globalRotations[1] * 180 / M_PI)) + "°, Psi: " + std::to_string(int(globalRotations[2] * 180 / M_PI)) + '°');
			statsText->setString("Vertices: " + std::to_string(vertexCount) + " | Edges: " + std::to_string(edgesDrawn) + " | Faces: " + std::to_string(triCount) + " | Faces Drawn: " + std::to_string(faceDraws));
			selectedMeshText->setString("Selected Mesh: " + std::to_string(selectedMeshIndex));

			
			for (int i = 0; i < texts.size(); i++)
			{
				window.draw(*texts[i]);
			}


			window.setTitle("3DEngine: FPS: " + std::to_string(currentFps));
			window.display();


			float frameTime = clock.restart().asMicroseconds();
			int frameWait = baseFrameTime - frameTime;
			if (frameTime > baseFrameTime || !capFrameRate)
			{
				currentFps = int(1000000.f / (frameTime));
			}
			else
			{
				currentFps = int(1000000.f / (frameTime + frameWait));
			}
			if (capFrameRate)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(frameWait));
			}
		}
	}
	return 0;
}


// Fits geometry coordinates to screen display.
std::vector<float> toScreenCoords(std::vector<float> axesCoordinates, std::vector<int> windowDimensions)
{
	std::vector<float> newCoords = { axesCoordinates[0] + windowDimensions[0] / 2,
								     windowDimensions[1] / 2 - axesCoordinates[1] };
	return newCoords;
}

// Performs 3D rotation by angles theta, phi and psi on a vertex's coordinates.
std::vector<float> rotate3D(std::vector<float> coordinates, float theta, float phi, float psi)
{
	Eigen::Vector3f oldCoordinates;
	oldCoordinates << coordinates[0], coordinates[1], coordinates[2];

	Eigen::Matrix3f xRot;
	Eigen::Matrix3f yRot;
	Eigen::Matrix3f zRot;
	xRot << 1, 0, 0,
			0, cos(phi), -sin(phi),
			0, sin(phi), cos(phi);
	yRot << cos(theta), 0, sin(theta),
			0, 1, 0,
			-sin(theta), 0, cos(theta);
	zRot << cos(psi), -sin(psi), 0,
			sin(psi), cos(psi), 0,
			0, 0, 1;
	
	Eigen::Vector3f transCoords;
	transCoords = xRot * yRot * zRot * oldCoordinates;
	std::vector<float> newCoords = { transCoords[0], transCoords[1], transCoords[2] };
	
	return newCoords;
}

