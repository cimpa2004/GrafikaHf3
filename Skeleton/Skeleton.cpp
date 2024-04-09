//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Dézsenyi Balázs Zoltán
// Neptun : JJSFIL
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
//kiinduló projekt: https://cg.iit.bme.hu/portal/oktatott-targyak/szamitogepes-grafika-es-kepfeldolgozas/grafikus-alap-hw/sw 

#include "framework.h"

// vertex shader in GLSL
const char* vertexSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0
	layout(location = 1) in vec2 vertexUV;			// Attrib Array 1

	out vec2 texCoord;								// output attribute

	void main() {
		texCoord = vertexUV;														// copy texture coordinates
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char* fragmentSource = R"(
	#version 330
    precision highp float;

	uniform sampler2D textureUnit;
	uniform int isGPUProcedural;

	in vec2 texCoord;			// variable input: interpolated texture coordinates
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

    int Mandelbrot(vec2 c) {
		vec2 z = c;
		for(int i = 10000; i > 0; i--) {
			z = vec2(z.x * z.x - z.y * z.y + c.x, 2 * z.x * z.y + c.y); // z_{n+1} = z_{n}^2 + c
			if (dot(z, z) > 4) return i;
		}
		return 0;
	}

	void main() {
		if (isGPUProcedural != 0) {
			int i = Mandelbrot(texCoord * 3 - vec2(2, 1.5)); 
			fragmentColor = vec4((i % 5)/5.0f, (i % 11) / 11.0f, (i % 31) / 31.0f, 1); 
		} else {
			fragmentColor = texture(textureUnit, texCoord);
		}
	}
)";

enum TextureFilteringMode {
	NEAREST = GL_NEAREST,
	LINEAR = GL_LINEAR
};
TextureFilteringMode currentFilterMode = LINEAR;
void setTextureFilteringMode(TextureFilteringMode mode) {
	currentFilterMode = mode;
	// Set the texture filtering mode
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
}
// 2D camera
class Camera2D {
	vec2 wCenter; // center in world coordinates
	vec2 wSize;   // width and height in world coordinates
public:
	Camera2D() : wCenter(20, 30), wSize(150, 150) { }

	mat4 V() { return TranslateMatrix(-wCenter); }
	mat4 P() { return ScaleMatrix(vec2(2 / wSize.x, 2 / wSize.y)); }

	mat4 Vinv() { return TranslateMatrix(wCenter); }
	mat4 Pinv() { return ScaleMatrix(vec2(wSize.x / 2, wSize.y / 2)); }
};

Camera2D camera;       // 2D camera
GPUProgram gpuProgram; // vertex and fragment shaders
int isGPUProcedural = (int)false;


class PoincareTexture {
	unsigned int textureID; 
	int width, height; 

	std::vector<vec3> getPointsOfLine(int r) {
		std::vector<vec3> points;
			vec3 p0(0, 0, 1);
			mat4 rotationMatrix = RotationMatrix(r, vec3(0, 0, 1));
			for (float t = 0.5; t <= 5.5/*5.5*/; t = t + 1) {
				vec3 point(p0 * coshf(t) + vec3(cos(r), sin(r), 0) * sinhf(t));
				// Sztereografikus vetítés 
				float scaleFactor = 1.0f / (1.0f + point.z);
				point.x *= scaleFactor;
				point.y *= scaleFactor;
				point.z = 0;
				points.push_back(point);
			}
		
		return points;
	}


	std::vector<vec3> GetEuklidesCircles() { // x,y,r
		std::vector<vec3> circles;
		std::vector<vec3> points = getPointsOfLine(0);
		for (int r = 0; r < 360; r += 40) {
			for (const auto& P : points) {
				float angle_rad = r * M_PI / 180.0f; 
				float x_rotated = P.x * cos(angle_rad) - P.y * sin(angle_rad);
				float y_rotated = P.x * sin(angle_rad) + P.y * cos(angle_rad);
				vec3 P_rotated(x_rotated, y_rotated, P.z);
				float d = P_rotated.x * P_rotated.x + P_rotated.y * P_rotated.y;
				vec3 Pinv = P_rotated / d;
				Pinv.z = 1;
				vec3 center = P_rotated + (Pinv - P_rotated) / 2.0f;
				center.z = 1;
				float radius = sqrtf(powf(P_rotated.x - center.x, 2) + powf(P_rotated.y - center.y, 2));
				circles.push_back(vec3(center.x, center.y, radius));
			}
		}
		return circles;
	}

	bool IsPointInCircle(vec3 point, vec3 Circle) {
		vec2 center = vec2(Circle.x, Circle.y);
		float radius = Circle.z;
		float distance = sqrtf(powf(point.x - center.x, 2) + powf(point.y - center.y, 2));
		return distance <= radius;
	}


public:
	PoincareTexture(int width, int height) : width(width), height(height) {
		textureID = 1;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, currentFilterMode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, currentFilterMode);
	}


	std::vector<vec4> RenderToTexture() {
		std::vector<vec4> image(width * height); 
		float centerX = width / 2.0f;
		float centerY = height / 2.0f;
		float radius = fmin(centerX, centerY) - 1.0f; 
		vec4 baseColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		std::vector<vec3> circles = GetEuklidesCircles();
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float cX = 2.0f * x / width - 1;
					float cY = 1.0f - 2.0f * y / height;
					int count = 0;
					float dx = x - centerX;
					float dy = y - centerY;
					float distance = sqrtf(dx * dx + dy * dy);
					vec3 currentPixel(cX, cY, 1);

					for (const vec3 &circle : circles) {
						if (IsPointInCircle(currentPixel, circle)) {
							count++;
						}
					}

					if (distance < radius) {

						if (count % 2 == 0) {
							image[y * width + x] = vec4(1.0f, 1.0f, 0.0f, 1.0f); 
						}
						else {
							image[y * width + x] = vec4(0.0f, 0.0f, 1.0f, 1.0f);
						}
					}
					else {
						image[y * width + x] = baseColor;
					}
				}
		}

		return image;
	}

	unsigned int GetTextureID() const { return textureID; }
};


class Star {
	unsigned int vao, vbo[2];
	vec2 vertices[10], uvs[10];
	Texture texture;
	int s = 40;
public:
	Star(int width, int height, const std::vector<vec4>& image) : texture(width, height, image) {
		vertices[0] = vec2(50, 30); uvs[0] = vec2(0.5, 0.5); 
		vertices[1] = vec2(90, 70);  uvs[1] = vec2(1, 1);
		vertices[2] = vec2(50, 30+s);   uvs[2] = vec2(0.5, 1);
		vertices[3] = vec2(10, 70);  uvs[3] = vec2(0, 1);
		vertices[4] = vec2(50-s, 30); uvs[4] = vec2(0, 0.5); 
		vertices[5] = vec2(10, -10);  uvs[5] = vec2(0, 0);
		vertices[6] = vec2(50, 30-s);   uvs[6] = vec2(0.5, 0);
		vertices[7] = vec2(90, -10);  uvs[7] = vec2(1, 0);
		vertices[8] = vec2(50+s, 30); uvs[8] = vec2(1, 0.5); 
		vertices[9] = vec2(90, 70);  uvs[9] = vec2(1, 1);
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);		

		glGenBuffers(2, vbo);	
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); 
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);    

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); 
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);     
	}

	void ChangeS(int value) { 
		s += value;
		vertices[2] = vec2(vertices[0].x, vertices[0].y + s);
		vertices[4] = vec2(vertices[0].x - s, vertices[0].y);
		vertices[6] = vec2(vertices[0].x, vertices[0].y - s);
		vertices[8] = vec2(vertices[0].x + s, vertices[0].y);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}

	

	void Draw() {
		mat4 MVPTransform = camera.V() * camera.P();
		gpuProgram.setUniform(MVPTransform, "MVP");
		gpuProgram.setUniform(isGPUProcedural, "isGPUProcedural");
		gpuProgram.setUniform(texture, "textureUnit");
		glBindVertexArray(vao);	
		glDrawArrays(GL_TRIANGLE_FAN, 0, 10);	
	}

	void RotateAroundOrigin(float angle) {
		float cosAngle = cosf(angle);
		float sinAngle = sinf(angle);

		vec2 origin = vertices[0];

		for (int i = 1; i < 10; ++i) {
			vec2 translated = vertices[i] - origin;
			float x = translated.x * cosAngle - translated.y * sinAngle;
			float y = translated.x * sinAngle + translated.y * cosAngle;
			vertices[i] = vec2(x, y) + origin;
		}


		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}
	void MoveOnCircularPath(float elapsedTime) {
		float angle = 2 * M_PI * (elapsedTime / 10.0f); 
		for (int i = 0; i < 10; ++i) {
			float deltaX = vertices[i].x - 20;
			float deltaY = vertices[i].y - 30;

			float newX = 20 + deltaX * cos(angle) - deltaY * sin(angle);
			float newY = 30 + deltaX * sin(angle) + deltaY * cos(angle);

			vertices[i].x = newX;
			vertices[i].y = newY;
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}

};

int resolution = 300;


Star* star; 
PoincareTexture* texture;
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	

	texture =  new PoincareTexture(resolution, resolution);
	star = new Star(resolution, resolution, texture->RenderToTexture());

	gpuProgram.create(vertexSource, fragmentSource, "fragmentColor");

	printf("\nUsage: \n");
	printf("Mouse Left Button: Pick and move vertex\n");
	printf("SPACE: Toggle between checkerboard (cpu) and Mandelbrot (gpu) textures\n");
}

bool animatonOn = false;

void onDisplay() {
	glClearColor(0, 0, 0, 0);						
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	star->Draw();
	glutSwapBuffers();									
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if(key == 'h') {
		star->ChangeS(-10);
		glutPostRedisplay();
	}
	else if (key == 'a') {
		animatonOn = true;
	}
	else if (key == 'r') {
		if (resolution > 100){
			resolution -= 100;
		}
		printf("Resolution: %d\n", resolution);
		texture = new PoincareTexture(resolution, resolution);
		star = new Star(resolution, resolution, texture->RenderToTexture());
		glutPostRedisplay();
	}
	else if (key == 'R') {
		resolution += 100;
		printf("Resolution: %d\n",resolution);
		texture = new PoincareTexture(resolution, resolution);
		star = new Star(resolution, resolution, texture->RenderToTexture());
		glutPostRedisplay();
	}
	else if (key == 't') {
		setTextureFilteringMode(NEAREST);

	}
	else if (key == 'T') {
		setTextureFilteringMode(LINEAR);

	}
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

bool mouseLeftPressed = false, mouseRightPressed = false;

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
	if (mouseLeftPressed) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
	}
	glutPostRedisplay();     // redraw
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) mouseLeftPressed = true;
		else					mouseLeftPressed = false;
	}
	if (button == GLUT_RIGHT_BUTTON) {
		if (state == GLUT_DOWN) mouseRightPressed = true;
		else					mouseRightPressed = false;
	}
	onMouseMotion(pX, pY);
}

void Animate() {
	if (animatonOn) {
		static long prevTime = glutGet(GLUT_ELAPSED_TIME);
		long currentTime = glutGet(GLUT_ELAPSED_TIME);
		float elapsedTime = (currentTime - prevTime) / 1000.0f;
		prevTime = currentTime;

		float rotationFrequencyAroundOrigin = 0.2f;

		float scaleFactor = 0.5f;

		float rotationAngleAroundOrigin = 2 * M_PI * rotationFrequencyAroundOrigin * elapsedTime * scaleFactor;

		star->RotateAroundOrigin(rotationAngleAroundOrigin);
		star->MoveOnCircularPath(elapsedTime);
	}
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME);
	Animate();
	glutPostRedisplay();
}