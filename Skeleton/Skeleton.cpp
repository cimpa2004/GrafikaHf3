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
	unsigned int textureID; // Az OpenGL textúra azonosítója
	int width, height; // Textúra mérete
	int resolution = 300;

	std::vector<vec3> getPointsOfLine(int r) {
		std::vector<vec3> points;
		vec3 p0 (0, 0, 1);
		for (float t = 0.5; t <= 5.5; t=t+1){
			vec3 point(p0*coshf(t)+ vec3(cos(r), sin(r),0)*sinhf(t));
			points.push_back(point);
			printf("x:%f, y:%f\n",point.x,point.y);
		}
		return points;
	}

	std::vector<vec3> GetEuklidesCircles(int r) {
		std::vector<vec3> circles;
		std::vector<vec3> points = getPointsOfLine(r);
		for (const auto& P : points) {
			float d = sqrtf(P.x * P.x + P.y * P.y);
			vec3 Pinv = P/ d;
			Pinv.z = 1;
			vec3 center = (P + Pinv) / 2.0f;
			center.z = 1;
			float radius = sqrtf(powf(P.x - center.x, 2) + powf(P.y - center.y, 2));
			circles.push_back(vec3(center.x, center.y, radius));
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
		// Textúra létrehozása OpenGL segítségével
		textureID = 1;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		/*glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);*/
	}

	//~PoincareTexture() {
	//	// Textúra felszabadítása
	//	glDeleteTextures(1, &textureID);
	//}

	std::vector<vec4> RenderToTexture() {
		std::vector<vec4> image(width * height); // A textúra képét tároló vektor

		// Poincaré kör textúra tartalmának kiszámítása
		float centerX = width / 2.0f;
		float centerY = height / 2.0f;
		float radius = fmin(centerX, centerY) - 1.0f; // Kör sugara, hogy ne lépje túl a kép méretét

		// Alapszín beállítása
		vec4 baseColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

		// Körök megszerzése az alap pontok alapján
		for (int r = 0; r <360; r += 40) {
			std::vector<vec3> circles = GetEuklidesCircles(r);
			
			// Kör alakú cellák színezése
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int count = 0;
					float dx = x - centerX;
					float dy = y - centerY;
					float distance = sqrtf(dx * dx + dy * dy);
					vec3 currentPixel(x, y, 1);

					// Iteráció minden körön és ellenõrzés, hogy a pixel azon belül van-e
					for (vec3 circle : circles) {
						if (IsPointInCircle(currentPixel, circle)) {
							count++;
						}
					}

					// Körön belüli vagy kívüli pont ellenõrzése
					if (distance < radius) {

						// A pont színezése az aktuális körök számától függõen
						if (count % 2 == 0) {
							image[y * width + x] = vec4(1.0f, 1.0f, 0.0f, 1.0f); // Sárga
						}
						else {
							image[y * width + x] = vec4(0.0f, 0.0f, 1.0f, 1.0f); // Kék
						}
					}
					else {
						// A körön kívül lévõ pont fekete
						image[y * width + x] = baseColor;
					}
				}
			}
		}

		return image;
	}






	// Getter függvények a textúra szélességének és magasságának lekérdezésére
	int GetWidth() const { return width; }
	int GetHeight() const { return height; }

	void setResolution(int v) { this->resolution = v; }

	// Getter függvény az OpenGL textúra azonosítójának lekérdezésére
	unsigned int GetTextureID() const { return textureID; }
};


class Star {
	unsigned int vao, vbo[2];
	vec2 vertices[10], uvs[10]; //4 csúcs és textura coord
	Texture texture;
	int s = 40;
public:
	Star(int width, int height, const std::vector<vec4>& image) : texture(width, height, image) {
		vertices[0] = vec2(50, 30); uvs[0] = vec2(0.5, 0.5); // teljes képet texturázzuk rá
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

		// vertex coordinates: vbo[0] -> Attrib Array 0 -> vertexPosition of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); 
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);    

		// vertex coordinates: vbo[1] -> Attrib Array 1 -> vertexUV of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); 
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);     
	}

	void ChangeS(int value) { //s lehet nulla de akkor eltûnik
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

		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 10);	// draw two triangles forming a quad
	}

	void RotateAroundOrigin(float angle) {
		// Calculate sine and cosine of the angle
		float cosAngle = cosf(angle);
		float sinAngle = sinf(angle);

		// Define the origin of rotation
		vec2 origin = vertices[0];

		// Iterate through all vertices (excluding the first one)
		for (int i = 1; i < 10; ++i) {
			// Translate the vertex to the origin
			vec2 translated = vertices[i] - origin;

			// Rotate the translated vertex
			float x = translated.x * cosAngle - translated.y * sinAngle;
			float y = translated.x * sinAngle + translated.y * cosAngle;

			// Translate the vertex back to its original position
			vertices[i] = vec2(x, y) + origin;
		}

		// Update GPU buffer data
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}
	void MoveOnCircularPath(float elapsedTime) {
		// Calculate the angle based on time elapsed
		float angle = 2 * M_PI * (elapsedTime / 10.0f); // 10 seconds for a full circle

		// Calculate new positions for each vertex
		for (int i = 0; i < 10; ++i) {
			// Calculate the displacement of the vertex relative to the center (20, 30)
			float deltaX = vertices[i].x - 20;
			float deltaY = vertices[i].y - 30;

			// Calculate the new position for the vertex using the circular path formula
			float newX = 20 + deltaX * cos(angle) - deltaY * sin(angle);
			float newY = 30 + deltaX * sin(angle) + deltaY * cos(angle);

			// Update the vertex position
			vertices[i].x = newX;
			vertices[i].y = newY;
		}

		// Update GPU buffer data
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}






};



Star* star; // The virtual world: one object
PoincareTexture* texture;
// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	int width = 128, height = 128;				// create checkerboard texture procedurally
	std::vector<vec4> image(width * height);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			float luminance = ((x / 16) % 2) ^ ((y / 16) % 2);
			image[y * width + x] = vec4(luminance, luminance, luminance, 1);
		}
	}
	

	texture =  new PoincareTexture(width, height);
	star = new Star(width, height, texture->RenderToTexture());

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "fragmentColor");

	printf("\nUsage: \n");
	printf("Mouse Left Button: Pick and move vertex\n");
	printf("SPACE: Toggle between checkerboard (cpu) and Mandelbrot (gpu) textures\n");
}

bool animatonOn = false;

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	star->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == ' ') {
		isGPUProcedural = !isGPUProcedural;
		glutPostRedisplay();// if d, invalidate display, i.e. redraw
	}
	else if(key == 'h') {
		star->ChangeS(-10);
		glutPostRedisplay();
	}
	else if (key == 'a') {
		animatonOn = true;
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
		float elapsedTime = (currentTime - prevTime) / 1000.0f; // Convert milliseconds to seconds
		prevTime = currentTime;

		// Calculate the rotation frequencies
		float rotationFrequencyAroundOrigin = 0.2f; // Rotations per second

		// Scale factor to adjust rotation speed
		float scaleFactor = 0.5f;

		// Calculate the rotation angles for this frame
		float rotationAngleAroundOrigin = 2 * M_PI * rotationFrequencyAroundOrigin * elapsedTime * scaleFactor;

		// Rotate the star around its origin
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