#include <iostream>
#include <string>
#include <assert.h>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

// Classe que representa câmera 3D com movimentação e controle de visão
class Camera {
public:
    vec3 position; // Posição da câmera no mundo
    vec3 front;
    vec3 up;
    vec3 right;
    vec3 worldUp; // Vetor up do mundo
    
    float yaw;
    float pitch;
    
    float movementSpeed;
    float mouseSensitivity;
    float fov;
    
    // Construtor da câmera com valores padrão
    Camera(vec3 position = vec3(0.0f, 0.0f, 3.0f), vec3 up = vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f) 
        : front(vec3(0.0f, 0.0f, -1.0f)), movementSpeed(2.5f), mouseSensitivity(0.1f), fov(45.0f) {
        this->position = position;
        this->worldUp = up;
        this->yaw = yaw;
        this->pitch = pitch;
        updateCameraVectors();
    }
    // Retorna a matriz de visualização 
    mat4 getViewMatrix() {
        return lookAt(position, position + front, up);
    }
    // Processa movimento do teclado (WASD)
    void processKeyboard(int direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (direction == GLFW_KEY_W)
            position += front * velocity;
        if (direction == GLFW_KEY_S)
            position -= front * velocity;
        if (direction == GLFW_KEY_A)
            position -= right * velocity;
        if (direction == GLFW_KEY_D)
            position += right * velocity;
    }
    // Processa movimento do mouse para controlar a câmera
    void processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;
        
        yaw += xoffset;
        pitch += yoffset;
        
        if (constrainPitch) {
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }
        
        updateCameraVectors();
    }
    // Processa scroll do mouse para zoom (alterar FOV)
    void processMouseScroll(float yoffset) {
        fov -= yoffset;
        if (fov < 1.0f)
            fov = 1.0f;
        if (fov > 45.0f)
            fov = 45.0f;
    }

private:
// Atualiza os vetores da câmera baseado nos ângulos yaw e pitch
    void updateCameraVectors() {
        vec3 newFront;
        newFront.x = cos(radians(yaw)) * cos(radians(pitch));
        newFront.y = sin(radians(pitch));
        newFront.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(newFront);
        
        right = normalize(cross(front, worldUp));
        up = normalize(cross(right, front));
    }
};
// Classe que representa modelo 3D
class Monkey {
public:
    vec3 position;
    vec3 scaling; 
    vec3 color;
    float rotationAngle;
    vec3 rotationAxis;
    float speed;
    // Construtor com valores padrão
    Monkey(vec3 pos = vec3(0.0f), vec3 scl = vec3(1.0f), vec3 col = vec3(1.0f)) 
        : position(pos), scaling(scl), color(col), rotationAngle(0.0f), rotationAxis(vec3(0.0f, 1.0f, 0.0f)), speed(1.0f) {}
    // Atualiza o ângulo de rotação baseado no tempo
    void update(float deltaTime) {
        rotationAngle += speed * deltaTime;
        if (rotationAngle > 360.0f) {
            rotationAngle -= 360.0f;
        }
    }
    // Retorna a matriz de modelo
    mat4 getModelMatrix() {
        mat4 model = mat4(1.0f);
        model = translate(model, position);
        model = rotate(model, radians(rotationAngle), rotationAxis);
        model = glm::scale(model, scaling); // <- usa a função glm::scale corretamente
        return model;
    }
};

// Declaração das funções de callback para teclado, mouse, rolagem e redimensionamento da janela
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// Declarações de funções utilitárias
int setupShader();
GLuint loadTexture(string filePath);
GLuint loadSuzanneModel(const string& objPath, int &nVertices);
void drawModel(GLuint shaderID, GLuint VAO, mat4 model, int nVertices, vec3 color = vec3(1.0, 0.0, 0.0));

// Dimensões da janela
const GLuint WIDTH = 800, HEIGHT = 800;

// Câmera e variáveis de controle de tempo e movimento do mouse
Camera camera;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Estrutura para representar uma luz com posição, cor, intensidade e estado
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    bool enabled;
};

// Instâncias das luzes: key, fill e back light 
Light keyLight;
Light fillLight;
Light backLight;
bool keyLightEnabled = true;
bool fillLightEnabled = true;
bool backLightEnabled = true;

// Vertex Shader - processa cada vértice e passa variáveis para o Fragment Shader
const GLchar *vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec3 vColor;

void main()
{
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;  
    TexCoord = texCoord;
    vColor = color;
    gl_Position = projection * view * model * vec4(position, 1.0);
})";

// Fragment Shader - calcula iluminação e cor final do pixel
const GLchar *fragmentShaderSource = R"(
#version 400
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec3 vColor;

uniform sampler2D texture_diffuse1;
uniform vec3 viewPos;

// Luzes
uniform vec3 keyLightPos;
uniform vec3 keyLightColor;
uniform float keyLightIntensity;
uniform bool keyLightEnabled;

uniform vec3 fillLightPos;
uniform vec3 fillLightColor;
uniform float fillLightIntensity;
uniform bool fillLightEnabled;

uniform vec3 backLightPos;
uniform vec3 backLightColor;
uniform float backLightIntensity;
uniform bool backLightEnabled;

uniform float ka;
uniform float kd;
uniform float ks;
uniform float shininess;

out vec4 FragColor;

// Função para calcular contribuição de uma luz
vec3 calculateLight(vec3 lightPos, vec3 lightColor, float lightIntensity, vec3 fragPos, vec3 normal, vec3 viewDir)
{
    // Vetor da superfície para a luz
    vec3 lightDir = normalize(lightPos - fragPos);
    
    // Atenuação baseada na distância
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
    
    // Difusa
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = kd * diff * lightColor * lightIntensity;
    
    // Especular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = ks * spec * lightColor * lightIntensity;
    
    return (diffuse + specular) * attenuation;
}

void main()
{
    vec3 ambient = ka * vec3(1.0, 1.0, 1.0);
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 result = ambient;
    
    if (keyLightEnabled)
        result += calculateLight(keyLightPos, keyLightColor, keyLightIntensity, FragPos, norm, viewDir);
    
    if (fillLightEnabled)
        result += calculateLight(fillLightPos, fillLightColor, fillLightIntensity, FragPos, norm, viewDir);
    
    if (backLightEnabled)
        result += calculateLight(backLightPos, backLightColor, backLightIntensity, FragPos, norm, viewDir);
    
    vec4 texColor = texture(texture_diffuse1, TexCoord);
    result = result * vColor * texColor.rgb;
    
    FragColor = vec4(result, 1.0);
})";

// Define posição, cor e intensidade das luzes baseadas na posição do objeto
void setupLights(vec3 objectPosition, float objectScale) {
    keyLight.position = objectPosition + vec3(2.0f, 2.0f, 2.0f) * objectScale;
    keyLight.color = vec3(1.0f, 1.0f, 1.0f);
    keyLight.intensity = 1.0f;
    keyLight.enabled = true;
    
    fillLight.position = objectPosition + vec3(-2.0f, 1.0f, 1.0f) * objectScale;
    fillLight.color = vec3(0.8f, 0.8f, 0.9f);
    fillLight.intensity = 0.5f;
    fillLight.enabled = true;
    
    backLight.position = objectPosition + vec3(0.0f, 1.0f, -2.0f) * objectScale;
    backLight.color = vec3(0.7f, 0.7f, 1.0f);
    backLight.intensity = 0.3f;
    backLight.enabled = true;
}

int main()
{
    glfwInit(); // Inicializa o janela GLFW
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "GB", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    
    // Define os callbacks para input e eventos
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    // Esconde cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Inicializa GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Print de informações da GPU e versão do OpenGL
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Carrega shaders, modelo e textura
    GLuint shaderID = setupShader();
    int nVertices;
    GLuint VAO = loadSuzanneModel("../assets/Modelos3D/Suzanne.obj", nVertices);
    GLuint textureID = loadTexture("../assets/Modelos3D/Suzanne.png");

    // Parâmetros do material ka kd ks
    float ka = 0.1f;
    float kd = 0.7f;
    float ks = 0.5f;
    float shininess = 32.0f;

    // Criação de dois macacos
    Monkey monkey1(vec3(-1.5f, 0.0f, 0.0f), vec3(0.8f), vec3(1.0f, 0.5f, 0.5f));
    Monkey monkey2(vec3(1.5f, 0.0f, 0.0f), vec3(0.8f), vec3(0.5f, 0.5f, 1.0f));

    
    // Define eixos e velocidade de rotação
    monkey1.rotationAxis = vec3(0.0f, 1.0f, 0.0f);
    monkey2.rotationAxis = vec3(1.0f, 0.0f, 0.0f);
    
    monkey1.speed = 45.0f;
    monkey2.speed = 30.0f;

    // Inicializa as luzes
    setupLights(vec3(0.0f, 0.0f, 0.0f), 1.0f);

    // Configura uniforms dos shaders
    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "texture_diffuse1"), 0);
    glUniform1f(glGetUniformLocation(shaderID, "ka"), ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), ks);
    glUniform1f(glGetUniformLocation(shaderID, "shininess"), shininess);

    // Define parâmetros das luzes
    glUniform3f(glGetUniformLocation(shaderID, "keyLightPos"), keyLight.position.x, keyLight.position.y, keyLight.position.z);
    glUniform3f(glGetUniformLocation(shaderID, "keyLightColor"), keyLight.color.r, keyLight.color.g, keyLight.color.b);
    glUniform1f(glGetUniformLocation(shaderID, "keyLightIntensity"), keyLight.intensity);

    glUniform3f(glGetUniformLocation(shaderID, "fillLightPos"), fillLight.position.x, fillLight.position.y, fillLight.position.z);
    glUniform3f(glGetUniformLocation(shaderID, "fillLightColor"), fillLight.color.r, fillLight.color.g, fillLight.color.b);
    glUniform1f(glGetUniformLocation(shaderID, "fillLightIntensity"), fillLight.intensity);

    glUniform3f(glGetUniformLocation(shaderID, "backLightPos"), backLight.position.x, backLight.position.y, backLight.position.z);
    glUniform3f(glGetUniformLocation(shaderID, "backLightColor"), backLight.color.r, backLight.color.g, backLight.color.b);
    glUniform1f(glGetUniformLocation(shaderID, "backLightIntensity"), backLight.intensity);
        
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    // Habilita z-buffer para profundidade
    glEnable(GL_DEPTH_TEST);

    // Loop principal
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Atualiza rotação dos macacos
        monkey1.update(deltaTime);
        monkey2.update(deltaTime);
        
        // Controle da câmera com teclas WASD
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_W, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_S, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_A, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard(GLFW_KEY_D, deltaTime);
        
        // Controle de movimentação dos objetos com setas e IJKL
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            monkey1.position.y += 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            monkey1.position.y -= 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            monkey1.position.x -= 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            monkey1.position.x += 1.0f * deltaTime;
            
      
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            monkey2.position.y += 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
            monkey2.position.y -= 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            monkey2.position.x -= 1.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
            monkey2.position.x += 1.0f * deltaTime;
        
        // Processa eventos
        glfwPollEvents();
        // Limpa a tela
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Atualiza a matriz de projeção e visão
        mat4 projection = perspective(radians(camera.fov), (float)width / (float)height, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));
        
        mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, value_ptr(view));
        
        glUniform3f(glGetUniformLocation(shaderID, "viewPos"), camera.position.x, camera.position.y, camera.position.z);

        // Renderiza os dois macacos
        drawModel(shaderID, VAO, monkey1.getModelMatrix(), nVertices, monkey1.color);
        drawModel(shaderID, VAO, monkey2.getModelMatrix(), nVertices, monkey2.color);
        // Atualiza estado das luzes
        glUniform1i(glGetUniformLocation(shaderID, "keyLightEnabled"), keyLightEnabled);
        glUniform1i(glGetUniformLocation(shaderID, "fillLightEnabled"), fillLightEnabled);
        glUniform1i(glGetUniformLocation(shaderID, "backLightEnabled"), backLightEnabled);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

// Callback chamada quando uma tecla é pressionada
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    // Fecha a janela com ESC
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (action == GLFW_PRESS) {
        switch (key) {
            // Alterna key light
            case GLFW_KEY_1:
                keyLightEnabled = !keyLightEnabled;
                cout << "Key light " << (keyLightEnabled ? "enabled" : "disabled") << endl;
                break;
            // Alterna fill light
            case GLFW_KEY_2:
                fillLightEnabled = !fillLightEnabled;
                cout << "Fill light " << (fillLightEnabled ? "enabled" : "disabled") << endl;
                break;
            // Alterna back light
            case GLFW_KEY_3:
                backLightEnabled = !backLightEnabled;
                cout << "Back light " << (backLightEnabled ? "enabled" : "disabled") << endl;
                break;
        }
    }
}

// Callback para movimento do mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        // Evita pulo no primeiro movimento
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    
    lastX = xpos;
    lastY = ypos;
    
    camera.processMouseMovement(xoffset, yoffset);
}

// Callback para scroll, zoom
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.processMouseScroll(yoffset);
}

// Callback chamada ao redimensionar a janela
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Compila e configura os shaders
int setupShader()
{
    // Compilação do vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Compilação do fragment shader  
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Carrega textura a partir de arquivo
GLuint loadTexture(string filePath)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrComponents, 0);
    
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << filePath << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// Estrutura que representa vértice
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

// Carrega modelo .OBJ e retorna VAO
GLuint loadSuzanneModel(const string& objPath, int &nVertices) {
    vector<vec3> temp_positions;
    vector<vec3> temp_normals;
    vector<vec2> temp_texcoords;
    vector<Vertex> vertices;

    ifstream file(objPath);
    if (!file.is_open()) {
        cerr << "Failed to open OBJ file: " << objPath << endl;
        return 0;
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string type;
        iss >> type;

        if (type == "v") {
            vec3 position;
            iss >> position.x >> position.y >> position.z;
            temp_positions.push_back(position);
        }
        else if (type == "vn") {
            vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        }
        else if (type == "vt") {
            vec2 texcoord;
            iss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            temp_texcoords.push_back(texcoord);
        }
        else if (type == "f") {
            string vertexData;
            while (iss >> vertexData) {
                Vertex vertex;
                istringstream viss(vertexData);
                string indexStr;
                unsigned int indices[3] = {0, 0, 0};
                int i = 0;

                while (getline(viss, indexStr, '/')) {
                    if (!indexStr.empty()) {
                        indices[i] = stoul(indexStr) - 1;
                    }
                    i++;
                    if (i >= 3) break;
                }

                if (indices[0] < temp_positions.size()) {
                    vertex.position = temp_positions[indices[0]];
                }
                if (indices[1] < temp_texcoords.size()) {
                    vertex.texCoord = temp_texcoords[indices[1]];
                }
                if (indices[2] < temp_normals.size()) {
                    vertex.normal = temp_normals[indices[2]];
                }
                vertices.push_back(vertex);
            }
        }
    }

    file.close();
    nVertices = vertices.size();

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    return VAO;
}

// Desenha o modelo usando o VAO e a matriz de modelo
void drawModel(GLuint shaderID, GLuint VAO, mat4 model, int nVertices, vec3 color)
{
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderID, "vColor"), color.r, color.g, color.b);
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, nVertices);
    glBindVertexArray(0);
}
