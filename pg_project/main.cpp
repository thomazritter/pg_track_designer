// Aluno: Thomaz Ritter
// Circuito de corrida com B-Spline fechado (1 VBO, 2 VAOs + Exportação OBJ)

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <filesystem>

// Variáveis globais
int windowWidth = 800, windowHeight = 600;
std::vector<glm::vec3> pontosDeControle;
std::vector<glm::vec3> pontosDaCurva;
std::vector<glm::vec3> curvaExterna, curvaInterna;

bool enterPressed = false;
bool k_pressed = false;
double PI = 3.1415926535;
GLint colorLoc;

const char* vertexShaderSource = R"(
    #version 400 core
    layout(location = 0) in vec3 aPos;
    uniform mat4 MVP;
    void main() {
        gl_Position = MVP * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 400 core
    out vec4 FragColor;
    uniform vec4 color;
    void main() {
        FragColor = color;
    }
)";

void gerarObj(const std::string& nomeArquivo = "pista.obj") {
    std::filesystem::path home = std::filesystem::path(std::getenv("HOME"));
    std::filesystem::path docPath = home / "Documents" / nomeArquivo;

    std::ofstream file(docPath);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir o arquivo OBJ." << std::endl;
        return;
    }

    int N = curvaInterna.size();

    file << "# OBJ gerado automaticamente\n";
    file << "mtllib pista.mtl\n";
    file << "g pista\n";
    file << "usemtl pista_material\n\n";

    // 1. Exportar vértices (2N)
    for (const auto& v : curvaInterna)
        file << "v " << v.x << " " << v.z << " " << v.y << "\n";
    for (const auto& v : curvaExterna)
        file << "v " << v.x << " " << v.z << " " << v.y << "\n";

    // 2. Coordenadas de textura
    file << "vt 0.0 0.0\n"; // 1
    file << "vt 1.0 0.0\n"; // 2
    file << "vt 1.0 1.0\n"; // 3
    file << "vt 0.0 1.0\n"; // 4

    // 3. Normais: calcular duas por quad - triangulos
    for (int i = 0; i < N; ++i) {
        glm::vec3 A = curvaInterna[i];
        glm::vec3 B = curvaInterna[(i + 1) % N];
        glm::vec3 C = curvaExterna[i];
        glm::vec3 D = curvaExterna[(i + 1) % N];

        glm::vec3 AB = B - A;
        glm::vec3 AC = C - A;
        glm::vec3 BC = C - B;
        glm::vec3 BD = D - B;

        glm::vec3 vn1 = glm::normalize(glm::cross(AB, AC));
        glm::vec3 vn2 = glm::normalize(glm::cross(BC, BD));

        file << "vn " << vn1.x << " " << vn1.y << " " << vn1.z << "\n";
        file << "vn " << vn2.x << " " << vn2.y << " " << vn2.z << "\n";
    }

    // 4. Faces (i, i+1, i+N, i+N+1)
    for (int i = 0; i < N; ++i) {
        int v1 = i + 1;                     // interna[i]
        int v2 = (i + 1) % N + 1;           // interna[i+1]
        int v3 = i + 1 + N;                 // externa[i]
        int v4 = (i + 1) % N + 1 + N;       // externa[i+1]

        int vn1 = i * 2 + 1;
        int vn2 = i * 2 + 2;

        // Face 1: interna[i]/1, interna[i+1]/2, externa[i+1]/3
        file << "f " << v1 << "/1/" << vn1 << " " << v2 << "/2/" << vn1 << " " << v4 << "/3/" << vn1 << "\n";

        // Face 2: externa[i]/4, interna[i]/1, externa[i+1]/3
        file << "f " << v3 << "/4/" << vn2 << " " << v1 << "/1/" << vn2 << " " << v4 << "/3/" << vn2 << "\n";
    }

    file.close();
    std::cout << "Arquivo OBJ exportado para: " << docPath << std::endl;
}


void gravarPontosDaCurvaEmTxt(const std::string& nomeArquivo = "pontoscurva.txt") {
    std::filesystem::path home = std::filesystem::path(std::getenv("HOME"));
    std::filesystem::path docPath = home / "Documents" / nomeArquivo;

    std::ofstream file(docPath);
    
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir o arquivo txt." << std::endl;
        return;
    }
    
    for (const auto& p : pontosDaCurva) {
        file << p.x << " " << p.z << " " << p.y << "\n";
    }
    
    file.close();
    std::cout << "Arquivo com pontos da curva salvo em " << docPath << std::endl;
}


void gerarCurvaBSpline() {
    pontosDaCurva.clear();
    int N = pontosDeControle.size();
    if (N < 4) return;

    float inc = 1.0f / 100.0f;

    for (int i = 0; i < N; ++i) {
        for (float t = 0; t <= 1.0f; t += inc) {
            float t2 = t * t;
            float t3 = t2 * t;

            glm::vec3 p0 = pontosDeControle[i % N];
            glm::vec3 p1 = pontosDeControle[(i + 1) % N];
            glm::vec3 p2 = pontosDeControle[(i + 2) % N];
            glm::vec3 p3 = pontosDeControle[(i + 3) % N];

            float b0 = (-t3 + 3 * t2 - 3 * t + 1);
            float b1 = (3 * t3 - 6 * t2 + 4);
            float b2 = (-3 * t3 + 3 * t2 + 3 * t + 1);
            float b3 = (t3);

            glm::vec3 ponto = (b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3) / 6.0f;
            pontosDaCurva.push_back(ponto);
        }
    }
}

void gerarInternaExterna() {
    curvaExterna.clear();
    curvaInterna.clear();
    int tamanho = pontosDaCurva.size();
    double PI2 = PI / 2.0;

    for (int i = 0; i < pontosDaCurva.size(); i += 2) {
        glm::vec2 A(pontosDaCurva[i].x, pontosDaCurva[i].y);
        glm::vec2 B(pontosDaCurva[(i + 2) % tamanho].x, pontosDaCurva[(i + 2) % tamanho].y);
        glm::vec2 AB = B - A;

        double tangente = AB.y / AB.x;
        double archtangente = atan(tangente);
        archtangente += (AB.x < 0) ? -PI2 : PI2;

        double cx = cos(archtangente) * 10;
        double cy = sin(archtangente) * 10;

        curvaInterna.push_back(glm::vec3(cx + A.x, cy + A.y, 0.0f));
        curvaExterna.push_back(glm::vec3(-cx + A.x, -cy + A.y, 0.0f));
    }
}

void desenharInternaExterna(GLuint shader, GLuint vaoInterna, GLuint vboInterna, GLuint vaoExterna, GLuint vboExterna) {
    glUseProgram(shader);

    GLint colorLoc = glGetUniformLocation(shader, "color");

    glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f); // Red
    glBindVertexArray(vaoInterna);
    glBindBuffer(GL_ARRAY_BUFFER, vboInterna);
    glBufferData(GL_ARRAY_BUFFER, curvaInterna.size() * sizeof(glm::vec3), curvaInterna.data(), GL_STATIC_DRAW);
    glDrawArrays(GL_LINE_STRIP, 0, curvaInterna.size());

    glUniform4f(colorLoc, 0.0f, 0.0f, 1.0f, 1.0f); // Blue
    glBindVertexArray(vaoExterna);
    glBindBuffer(GL_ARRAY_BUFFER, vboExterna);
    glBufferData(GL_ARRAY_BUFFER, curvaExterna.size() * sizeof(glm::vec3), curvaExterna.data(), GL_STATIC_DRAW);
    glDrawArrays(GL_LINE_STRIP, 0, curvaExterna.size());
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && !enterPressed) {
        enterPressed = true;
    }
    if (key == GLFW_KEY_K && action == GLFW_PRESS && !k_pressed){
        k_pressed = true;
    }
    
}

void resize(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void setupMac() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
}

void callbackDoMouse(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        double x, y;
        glfwGetCursorPos(glfwGetCurrentContext(), &x, &y);
        pontosDeControle.push_back(glm::vec3(x, windowHeight - y, 0));
    }
}

void desenharCliqueDoMouse(GLuint shader, GLuint vao, GLuint vbo) {
    glUniform4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    glUseProgram(shader);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glDrawArrays(GL_POINTS, 0, pontosDeControle.size());
}

void desenharLinhaEntrePontos(GLuint shader, GLuint vao, GLuint vbo) {
    glUniform4f(colorLoc, 0.7f, 0.7f, 0.7f, 1.0f); // Light Gray
    glUseProgram(shader);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glDrawArrays(GL_LINE_STRIP, 0, pontosDeControle.size());
}

void desenharCurvaBSpline(GLuint shader, GLuint vao, GLuint vbo) {
    glUniform4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f); // Green
    glUseProgram(shader);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glDrawArrays(GL_LINE_STRIP, 0, pontosDaCurva.size());
}

int main() {
    glfwInit();
    setupMac();

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Circuito B-Spline Fechado", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, resize);
    glfwSetMouseButtonCallback(window, callbackDoMouse);
    glfwSetKeyCallback(window, key_callback);

    glewExperimental = GL_TRUE;
    glewInit();

    GLuint vaoPontosDeControle, vaoLinhasDeControle, vaoPontosDaCurva, vaoInterna, vaoExterna;
    GLuint vboPontosDeControle, vboLinhasDeControle, vboPontosDaCurva, vboInterna, vboExterna;

    glGenVertexArrays(1, &vaoPontosDeControle);
    glGenVertexArrays(1, &vaoLinhasDeControle);
    glGenVertexArrays(1, &vaoPontosDaCurva);
    glGenVertexArrays(1, &vaoInterna);
    glGenVertexArrays(1, &vaoExterna);

    glGenBuffers(1, &vboPontosDeControle);
    glGenBuffers(1, &vboLinhasDeControle);
    glGenBuffers(1, &vboPontosDaCurva);
    glGenBuffers(1, &vboInterna);
    glGenBuffers(1, &vboExterna);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(vs);
    glCompileShader(fs);

    GLuint shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint mvpLoc = glGetUniformLocation(shader, "MVP");
    colorLoc = glGetUniformLocation(shader, "color");

    glBindVertexArray(vaoPontosDeControle);
    glBindBuffer(GL_ARRAY_BUFFER, vboPontosDeControle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glBindVertexArray(vaoLinhasDeControle);
    glBindBuffer(GL_ARRAY_BUFFER, vboLinhasDeControle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glBindVertexArray(vaoPontosDaCurva);
    glBindBuffer(GL_ARRAY_BUFFER, vboPontosDaCurva);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glBindVertexArray(vaoInterna);
    glBindBuffer(GL_ARRAY_BUFFER, vboInterna);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glBindVertexArray(vaoExterna);
    glBindBuffer(GL_ARRAY_BUFFER, vboExterna);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader);
        glm::mat4 proj = glm::ortho(0.0f, float(windowWidth), 0.0f, float(windowHeight));
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &proj[0][0]);

        glBindBuffer(GL_ARRAY_BUFFER, vboPontosDeControle);
        glBufferData(GL_ARRAY_BUFFER, pontosDeControle.size() * sizeof(glm::vec3), pontosDeControle.data(), GL_STATIC_DRAW);

        glPointSize(5.0f);
        desenharCliqueDoMouse(shader, vaoPontosDeControle, vboPontosDeControle);

        if (pontosDeControle.size() >= 2) {
            glBindBuffer(GL_ARRAY_BUFFER, vboLinhasDeControle);
            glBufferData(GL_ARRAY_BUFFER, pontosDeControle.size() * sizeof(glm::vec3), pontosDeControle.data(), GL_STATIC_DRAW);
            desenharLinhaEntrePontos(shader, vaoLinhasDeControle, vboLinhasDeControle);
        }

        if (pontosDeControle.size() > 4) {
            gerarCurvaBSpline();
            glBindBuffer(GL_ARRAY_BUFFER, vboPontosDaCurva);
            glBufferData(GL_ARRAY_BUFFER, pontosDaCurva.size() * sizeof(glm::vec3), pontosDaCurva.data(), GL_STATIC_DRAW);
            desenharCurvaBSpline(shader, vaoPontosDaCurva, vboPontosDaCurva);
        }

        if (enterPressed) {
            gerarInternaExterna();
            desenharInternaExterna(shader, vaoInterna, vboInterna, vaoExterna, vboExterna);
        }
        
        if(enterPressed && k_pressed) {
            gerarObj();
            gravarPontosDaCurvaEmTxt();
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &vaoPontosDeControle);
    glDeleteVertexArrays(1, &vaoLinhasDeControle);
    glDeleteVertexArrays(1, &vaoPontosDaCurva);
    glDeleteBuffers(1, &vboPontosDeControle);
    glDeleteBuffers(1, &vboLinhasDeControle);
    glDeleteBuffers(1, &vboPontosDaCurva);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
