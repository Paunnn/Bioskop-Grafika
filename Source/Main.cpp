#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <direct.h> 
#include "../Header/Util.h"


const int ROWS = 5;
const int COLS = 10;
const int TOTAL_SEATS = ROWS * COLS;
const float TARGET_FPS = 75.0f;
const float FRAME_TIME = 1.0f / TARGET_FPS;


enum SeatStatus { FREE, RESERVED, BOUGHT };
enum AppState { WAITING, MOVIE, LEAVING };


struct Seat {
    float x, y;
    SeatStatus status;
    Seat() : x(0), y(0), status(FREE) {}
    Seat(float px, float py) : x(px), y(py), status(FREE) {}
};

struct Person {
    float x, y;
    float targetX, targetY;
    bool seated;
    bool leaving;
    Person() = default;
    Person(float tx, float ty) : targetX(tx), targetY(ty), seated(false), leaving(false) {
        x = -0.95f;
        y = 0.9f;
    }
};


std::vector<Seat> seats;
std::vector<Person> people;
AppState currentState = WAITING;
float movieStartTime = -1.0f;
int frameCounter = 0;
float screenR = 1.0f, screenG = 1.0f, screenB = 1.0f;
bool doorOpen = false;
float doorAnimationProgress = 0.0f;

unsigned int shaderProgram = 0;
unsigned int quadVAO = 0;
unsigned int quadVBO = 0;
GLFWcursor* cameraCursor = nullptr;

GLint uniPos = -1, uniSize = -1, uniColor = -1;

unsigned int textureVAO = 0;
unsigned int textureVBO = 0;
unsigned int studentTexture = 0;
unsigned int textureShaderProgram = 0;


int endProgram(const char* message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

void initSeats() {
    seats.resize(TOTAL_SEATS);
    float startX = -0.7f;
    float startY = 0.4f;
    float spacingX = 0.15f;
    float spacingY = 0.15f;

    int idx = 0;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            float x = startX + c * spacingX;
            float y = startY - r * spacingY;
            seats[idx++] = Seat(x, y);
        }
    }
}

bool initRenderer(const char* vertPath, const char* fragPath) {
    shaderProgram = createShader(vertPath, fragPath);
    if (!shaderProgram) return false;

    uniPos = glGetUniformLocation(shaderProgram, "uPos");
    uniSize = glGetUniformLocation(shaderProgram, "uSize");
    uniColor = glGetUniformLocation(shaderProgram, "uColor");

    float quadVertices[] = {
        -0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

bool initTextureRenderer() {
    
    const char* texVertShader = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        
        uniform vec2 uPos;
        uniform vec2 uSize;
        
        out vec2 TexCoord;
        
        void main() {
            vec2 pos = aPos * uSize + uPos;
            gl_Position = vec4(pos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    
    const char* texFragShader = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        
        uniform sampler2D uTexture;
        
        void main() {
            FragColor = texture(uTexture, TexCoord);
        }
    )";
    
    
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &texVertShader, NULL);
    glCompileShader(vertexShader);
    
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &texFragShader, NULL);
    glCompileShader(fragmentShader);
    
   
    textureShaderProgram = glCreateProgram();
    glAttachShader(textureShaderProgram, vertexShader);
    glAttachShader(textureShaderProgram, fragmentShader);
    glLinkProgram(textureShaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
   
    float textureQuadVertices[] = {
        
        -0.5f,  0.5f,   0.0f, 1.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &textureVAO);
    glGenBuffers(1, &textureVBO);
    
    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(textureQuadVertices), textureQuadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    return true;
}

void drawRectangle(float x, float y, float width, float height, float r, float g, float b, float a) {
    if (uniPos != -1) glUniform2f(uniPos, x, y);
    if (uniSize != -1) glUniform2f(uniSize, width, height);
    if (uniColor != -1) glUniform4f(uniColor, r, g, b, a);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void drawSeats() {
    for (int i = 0; i < TOTAL_SEATS; ++i) {
        float r, g, b;
        switch (seats[i].status) {
        case FREE: r = 0.2f; g = 0.5f; b = 1.0f; break;
        case RESERVED: r = 1.0f; g = 1.0f; b = 0.0f; break;
        case BOUGHT: r = 1.0f; g = 0.0f; b = 0.0f; break;
        }
        drawRectangle(seats[i].x, seats[i].y, 0.08f, 0.08f, r, g, b, 1.0f);
    }
}

void drawScreen() {
    drawRectangle(0.0f, 0.85f, 1.4f, 0.15f, screenR, screenG, screenB, 1.0f);
}

void drawOverlay() {
    if (!doorOpen && doorAnimationProgress < 0.1f) {
        drawRectangle(0.0f, 0.0f, 2.0f, 2.0f, 0.2f, 0.2f, 0.2f, 0.5f);
    }
}

void drawStudentLogo() {
    if (!studentTexture || !textureShaderProgram) return;
    
    glUseProgram(textureShaderProgram);
    
    GLint uniTexPos = glGetUniformLocation(textureShaderProgram, "uPos");
    GLint uniTexSize = glGetUniformLocation(textureShaderProgram, "uSize");
    
    
    glUniform2f(uniTexPos, 0.75f, -0.75f);
    glUniform2f(uniTexSize, 0.2f, 0.2f);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, studentTexture);
    glUniform1i(glGetUniformLocation(textureShaderProgram, "uTexture"), 0);
    
    glBindVertexArray(textureVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void drawDoor(float deltaTime) {
    const float DOOR_SPEED = 2.0f;
    if (doorOpen && doorAnimationProgress < 1.0f) {
        doorAnimationProgress += DOOR_SPEED * deltaTime;
        if (doorAnimationProgress > 1.0f) doorAnimationProgress = 1.0f;
    }
    else if (!doorOpen && doorAnimationProgress > 0.0f) {
        doorAnimationProgress -= DOOR_SPEED * deltaTime;
        if (doorAnimationProgress < 0.0f) doorAnimationProgress = 0.0f;
    }

    float doorOffset = doorAnimationProgress * 0.2f;
    float doorHeight = 0.25f * (1.0f - doorAnimationProgress);

    if (doorHeight > 0.01f) {
        drawRectangle(-0.85f, 0.75f + doorOffset, 0.2f, doorHeight, 0.5f, 0.4f, 0.3f, 1.0f);
    }
}

void drawPerson(float x, float y) {
    drawRectangle(x, y - 0.02f, 0.03f, 0.06f, 0.8f, 0.6f, 0.4f, 1.0f);
    drawRectangle(x, y + 0.03f, 0.025f, 0.025f, 1.0f, 0.8f, 0.6f, 1.0f);
}

void drawPeople() {
    for (const auto& p : people) {
        drawPerson(p.x, p.y);
    }
}

bool findNAdjacentSeats(int n, std::vector<int>& seatIndices) {
    seatIndices.clear();
    for (int row = ROWS - 1; row >= 0; --row) {
        for (int col = COLS - n; col >= 0; --col) {
            bool found = true;
            std::vector<int> temp;
            for (int i = 0; i < n; ++i) {
                int idx = row * COLS + col + i;
                if (seats[idx].status != FREE) {
                    found = false;
                    break;
                }
                temp.push_back(idx);
            }
            if (found) {
                seatIndices = temp;
                return true;
            }
        }
    }
    return false;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (currentState != WAITING || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    float xNorm = (xpos / (float)windowWidth) * 2.0f - 1.0f;
    float yNorm = -((ypos / (float)windowHeight) * 2.0f - 1.0f);

    for (int i = 0; i < TOTAL_SEATS; ++i) {
        float dx = xNorm - seats[i].x;
        float dy = yNorm - seats[i].y;
        if (fabs(dx) < 0.05f && fabs(dy) < 0.05f) {
            if (seats[i].status == FREE) {
                seats[i].status = RESERVED;
            }
            else if (seats[i].status == RESERVED) {
                seats[i].status = FREE;
            }
            break;
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (currentState != WAITING || action != GLFW_PRESS) return;

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        int n = key - GLFW_KEY_0;
        std::vector<int> indices;
        if (findNAdjacentSeats(n, indices)) {
            for (int idx : indices) seats[idx].status = BOUGHT;
            std::cout << "Kupljeno " << n << " karata." << std::endl;
        }
        else {
            std::cout << "Nema " << n << " susednih slobodnih sedista!" << std::endl;
        }
    }

    if (key == GLFW_KEY_ENTER) {
        int totalOccupied = 0;
        for (const auto& s : seats) {
            if (s.status == RESERVED || s.status == BOUGHT) totalOccupied++;
        }

        if (totalOccupied == 0) {
            std::cout << "Nema rezervisanih ili kupljenih sedista!" << std::endl;
            return;
        }

        people.clear();
        for (int i = 0; i < TOTAL_SEATS; ++i) {
            if (seats[i].status == RESERVED || seats[i].status == BOUGHT) {
                people.emplace_back(seats[i].x, seats[i].y);
            }
        }

        int numPeople = 1 + rand() % (int)people.size();
        people.resize(numPeople);

        currentState = MOVIE;
        movieStartTime = (float)glfwGetTime();
        doorOpen = true;
        frameCounter = 0;
        std::cout << "Projekcija pocinje! Ulazi " << numPeople << " ljudi." << std::endl;
    }
}

void updatePeople(float deltaTime) {
    const float SPEED = 0.5f;
    bool allSeated = true;
    bool allGone = true;

    for (auto& p : people) {
        if (!p.leaving) {
            if (!p.seated) {
                float dy = p.targetY - p.y;
                if (fabs(dy) > 0.01f) {
                    p.y += (dy > 0 ? 1 : -1) * SPEED * deltaTime;
                    allSeated = false;
                }
                else {
                    float dx = p.targetX - p.x;
                    if (fabs(dx) > 0.01f) {
                        p.x += (dx > 0 ? 1 : -1) * SPEED * deltaTime;
                        allSeated = false;
                    }
                    else {
                        p.seated = true;
                    }
                }
            }
        }
        else {
            float dx = -0.95f - p.x;
            if (fabs(dx) > 0.01f) {
                p.x += (dx > 0 ? 1 : -1) * SPEED * deltaTime;
                allGone = false;
            }
            else {
                float dy = 0.9f - p.y;
                if (fabs(dy) > 0.01f) {
                    p.y += (dy > 0 ? 1 : -1) * SPEED * deltaTime;
                    allGone = false;
                }
            }
        }
    }

    if (allSeated && doorOpen && currentState == MOVIE) {
        doorOpen = false;
        std::cout << "Svi su seli. Film pocinje!" << std::endl;
    }

    if (allGone && currentState == LEAVING) {
        doorOpen = false;
        currentState = WAITING;
        people.clear();
        for (auto& s : seats) s.status = FREE;
        std::cout << "Svi su izasli. Sistem resetovan." << std::endl;
    }
}

int main() {
    srand((unsigned int)time(NULL));

    if (!glfwInit()) return endProgram("GLFW init failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height,
        "Bioskop - Upravljanje Sedistima",
        monitor, NULL);
    if (!window) return endProgram("Window creation failed.");

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); 

    cameraCursor = loadImageToCursor("camera.png");
    if (cameraCursor) glfwSetCursor(window, cameraCursor);

    if (glewInit() != GLEW_OK) return endProgram("GLEW init failed.");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!initRenderer("basic.vert", "basic.frag")) {
        return endProgram("Shader load failed.");
    }

    initTextureRenderer();
    studentTexture = loadTexture("student.png");
    if (!studentTexture) {
        std::cout << "Upozorenje: Nije moguce ucitati student.png" << std::endl;
    }

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    initSeats();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    float lastTime = (float)glfwGetTime();
    float accumulator = 0.0f;

    std::cout << "========================================" << std::endl;
    std::cout << "BIOSKOP - KONTROLE" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Levi klik: Rezervisi/Otkazi sediste" << std::endl;
    std::cout << "1-9: Kupi N karata" << std::endl;
    std::cout << "Enter: Pokreni projekciju" << std::endl;
    std::cout << "Escape: Zatvori" << std::endl;
    std::cout << "========================================" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        accumulator += deltaTime;

        if (accumulator >= FRAME_TIME) {
            accumulator -= FRAME_TIME;

            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(shaderProgram);

            drawScreen();
            drawSeats();
            drawDoor(FRAME_TIME);

            if (currentState == MOVIE || currentState == LEAVING) {
                drawPeople();
                updatePeople(FRAME_TIME);
            }

            drawOverlay();

            drawStudentLogo();
           
            if (currentState == MOVIE && !doorOpen && doorAnimationProgress < 0.1f) {
                float elapsed = currentTime - movieStartTime;

                frameCounter++;
                if (frameCounter >= 20) {
                    screenR = 0.2f + (float)rand() / RAND_MAX * 0.8f;
                    screenG = 0.2f + (float)rand() / RAND_MAX * 0.8f;
                    screenB = 0.2f + (float)rand() / RAND_MAX * 0.8f;
                    frameCounter = 0;
                }

                if (elapsed >= 20.0f) {
                    std::cout << "Film zavrsen. Ljudi izlaze..." << std::endl;
                    screenR = screenG = screenB = 1.0f;
                    doorOpen = true;
                    currentState = LEAVING;
                    for (auto& p : people) {
                        p.leaving = true;
                        p.seated = false;
                    }
                }
            }

            glfwSwapBuffers(window);
        }

        glfwPollEvents();
    }

    if (cameraCursor) glfwDestroyCursor(cameraCursor);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (textureVBO) glDeleteBuffers(1, &textureVBO);
    if (textureVAO) glDeleteVertexArrays(1, &textureVAO);
    if (studentTexture) glDeleteTextures(1, &studentTexture);
    if (textureShaderProgram) glDeleteProgram(textureShaderProgram);
    if (shaderProgram) glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}