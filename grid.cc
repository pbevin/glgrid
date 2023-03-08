#include <iostream>
#include <vector>
#include <algorithm>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using glm::mat4;
using glm::normalize;
using glm::perspective;
using glm::radians;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using std::cout;
using std::endl;
using std::vector;

void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// grid dimensions
const unsigned int GRID_WIDTH = 1000;
const unsigned int GRID_HEIGHT = 1000;

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool realTimeUpdating = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// TO DO a simple camera class
vec3 cameraPos = vec3(0.0f, 0.0f, 0.0f);
vec3 cameraFront = vec3(0, 0, -1);
vec3 cameraRight = vec3(1, 0, 0);
vec3 cameraUp = vec3(0, 1, 0);
mat4 model = mat4(1.0);
mat4 view;
mat4 projection;
float scrollSpeed = 2.0f;
float fov = 90.0f;
float nearDist = 0.1f;
float farDist = 1000.0f;
float ar = (float)SCR_WIDTH / (float)SCR_HEIGHT;

class LineRenderer
{
    int shaderProgram;
    unsigned int VBO, VAO;
    mat4 viewProjection;

    vector<float> vertices;

public:
    LineRenderer()
    {

        const char *vertexShaderSource = "#version 330 core\n"
                                         "layout (location = 0) in vec3 aPos;\n"
                                         "uniform mat4 viewProjection;\n"
                                         "void main()\n"
                                         "{\n"
                                         "   gl_Position = viewProjection * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                         "}\0";
        const char *fragmentShaderSource = "#version 330 core\n"
                                           "out vec4 FragColor;\n"
                                           "void main()\n"
                                           "{\n"
                                           "   FragColor = vec4(0,0,0,1);\n"
                                           "}\n\0";

        // vertex shader
        int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                 << infoLog << endl;
        }
        // fragment shader
        int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                 << infoLog << endl;
        }
        // link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // check for linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                 << infoLog << endl;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        vector<float> placeHolderVertices = {};

        vertices.insert(vertices.end(), placeHolderVertices.begin(), placeHolderVertices.end());
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void setCamera(mat4 cameraMatrix)
    {
        viewProjection = cameraMatrix;
    }

    void addLine(vec3 start, vec3 end)
    {
        vector<float> lineVertices = {
            start.x,
            start.y,
            start.z,
            end.x,
            end.y,
            end.z,

        };

        vertices.insert(vertices.end(), lineVertices.begin(), lineVertices.end());
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
    }

    int draw()
    {
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProjection"), 1, GL_FALSE, &viewProjection[0][0]);

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, vertices.size() / 3);
        return 0;
    }
};

// maths functions to pick which grid (not necessary, just for demonstration purposes)

vec3 rayCast(double xpos, double ypos, mat4 projection, mat4 view)
{
    // converts a position from the 2d xpos, ypos to a normalized 3d direction
    float x = (2.0f * xpos) / SCR_WIDTH - 1.0f;
    float y = 1.0f - (2.0f * ypos) / SCR_HEIGHT;
    float z = 1.0f;
    vec3 ray_nds = vec3(x, y, z);
    vec4 ray_clip = vec4(ray_nds.x, ray_nds.y, -1.0f, 1.0f);
    // eye space to clip we would multiply by projection so
    // clip space to eye space is the inverse projection
    vec4 ray_eye = inverse(projection) * ray_clip;
    // convert point to forwards
    ray_eye = vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
    // world space to eye space is usually multiply by view so
    // eye space to world space is inverse view
    vec4 inv_ray_wor = (inverse(view) * ray_eye);
    vec3 ray_wor = vec3(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
    ray_wor = normalize(ray_wor);
    return ray_wor;
}

vec3 rayPlaneIntersection(vec3 ray_position, vec3 ray_direction, vec3 plane_normal, vec3 plane_position)
{
    float d = dot(plane_normal, plane_position - ray_position) / (0.001 + dot(ray_direction, plane_normal));
    return ray_position + ray_direction * d;
}

template <typename T>
vector<T> flatten(const vector<vector<T>> &orig, vec2 bottomLeft = vec2(0, 0), vec2 topRight = vec2(GRID_WIDTH, GRID_HEIGHT))
{
    vector<T> ret;
    // for(const auto &v: orig)
    //     ret.insert(ret.end(), v.begin(), v.end());
    float rx = (topRight.x >= GRID_WIDTH ? GRID_WIDTH : topRight.x + 1);
    float ry = (topRight.y >= GRID_HEIGHT ? GRID_HEIGHT : topRight.y + 1);

    float lx = (bottomLeft.x <= 0 ? 0 : bottomLeft.x);
    float ly = (bottomLeft.y <= 0 ? 0 : bottomLeft.y);

    for (int i = lx; i < rx; i++)
    {
        vector<T> v = orig[i];
        ret.insert(ret.end(), v.begin() + ly, v.begin() + ry);
    }
    return ret;
}

class QuadRenderer
{

public:
    unsigned int shaderProgram;
    unsigned int VBO, VAO, EBO;

    unsigned int matrixBuffer;
    unsigned int colorBuffer;

    vector<vector<vec3>> colors;
    vector<vector<vec3>> frustumCulledColors;
    vector<vec3> _colors;

    vector<vector<mat4>> models;
    vector<vector<mat4>> frustumCulledModels;
    vector<mat4> _models;

    mat4 viewProjection;

    vec2 bottomLeft = vec2(0, 0);
    vec2 topRight = vec2(GRID_WIDTH, GRID_HEIGHT);

    QuadRenderer()
    {

        // create an empty grid
        colors.resize(GRID_WIDTH);
        for (int j = 0; j < GRID_WIDTH; j++)
        {
            colors[j].resize(GRID_HEIGHT);
            std::fill(colors[j].begin(), colors[j].end(), vec3(0));
        }

        models.resize(GRID_WIDTH);
        for (int j = 0; j < GRID_WIDTH; j++)
        {
            models[j].resize(GRID_HEIGHT);
            std::fill(models[j].begin(), models[j].end(), mat4(0));
        }

        const char *vertexShaderSource = "#version 330 core\n"
                                         "layout (location = 0) in vec3 aPos;\n"
                                         "layout (location = 1) in mat4 aInstanceMatrix;\n"
                                         "layout (location = 5) in vec3 aCol\n;"
                                         "uniform mat4 viewProjection;\n"
                                         "out vec3 color;\n"
                                         "void main()\n"
                                         "{\n"
                                         "   gl_Position = viewProjection * aInstanceMatrix * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                         "   color = aCol;\n"
                                         "}\0";
        const char *fragmentShaderSource = "#version 330 core\n"
                                           "out vec4 FragColor;\n"
                                           "in vec3 color;\n"
                                           "void main()\n"
                                           "{\n"
                                           "   FragColor = vec4(color,1);\n"
                                           "}\n\0";

        // build and compile our shader program
        // ------------------------------------
        // vertex shader
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                 << infoLog << endl;
        }
        // fragment shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                 << infoLog << endl;
        }
        // link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // check for linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                 << infoLog << endl;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        float vertices[] = {
            1.0f, 1.0f, 0.0f, // top right
            1.0f, 0.0f, 0.0f, // bottom right
            0.0f, 0.0f, 0.0f, // bottom left
            0.0f, 1.0f, 0.0f  // top left
        };

        unsigned int indices[] = {
            // note that we start from 0!
            0, 1, 3, // first Triangle
            1, 2, 3  // second Triangle
        };

        _models = flatten(models);
        _colors = flatten(colors);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glGenBuffers(1, &matrixBuffer);
        glGenBuffers(1, &colorBuffer);

        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
        glBufferData(GL_ARRAY_BUFFER, _models.size() * sizeof(mat4), &_models.front(), GL_STATIC_DRAW);

        // set attribute pointers for matrix (4 times vec4)

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)0);
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(vec4)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(2 * sizeof(vec4)));
        glEnableVertexAttribArray(3);

        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(3 * sizeof(vec4)));
        glEnableVertexAttribArray(4);

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);

        glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);

        glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
        glBufferData(GL_ARRAY_BUFFER, _colors.size() * sizeof(vec3), &_colors.front(), GL_STATIC_DRAW);

        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(5);
        glVertexAttribDivisor(5, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void calculateFrustum()
    {
        vec3 rayWorld = rayCast(0, SCR_HEIGHT, projection, view);
        vec3 worldPos = rayPlaneIntersection(cameraPos, rayWorld, vec3(0, 0, 1), vec3(0, 0, 0));
        bottomLeft = vec2((int)worldPos.x, (int)worldPos.y);

        rayWorld = rayCast(SCR_WIDTH, 0, projection, view);
        worldPos = rayPlaneIntersection(cameraPos, rayWorld, vec3(0, 0, 1), vec3(0, 0, 0));
        topRight = vec2((int)worldPos.x, (int)worldPos.y);
    }

    // send updated data to GPU
    void update()
    {
        _models = flatten(models, bottomLeft, topRight);
        _colors = flatten(colors, bottomLeft, topRight);

        vector<mat4> shadedCellModels = {};
        vector<vec3> shadedCellColors = {};

        for (int i = 0; i < _models.size(); i++)
        {
            // only send initialized cells to the GPU
            if (_models[i] != mat4(0) && _colors[i] != vec3(0))
            {
                shadedCellModels.push_back(_models[i]);
                shadedCellColors.push_back(_colors[i]);
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, shadedCellModels.size() * sizeof(mat4), &shadedCellModels.front());

        glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, shadedCellColors.size() * sizeof(vec3), &shadedCellColors.front());
    }

    void addQuad(vec2 pos, vec3 col)
    {

        mat4 model = translate(mat4(1.0), vec3(pos, 0.0));
        models[(int)pos.x][(int)pos.y] = model;
        colors[(int)pos.x][(int)pos.y] = col;
    }

    void remove(vec2 pos)
    {
        // change color to white
        colors[(int)pos.x][(int)pos.y] = vec3(1);
    }

    void setCamera(mat4 cameraMatrix)
    {
        viewProjection = cameraMatrix;
    }

    void draw()
    {

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProjection"), 1, GL_FALSE, &viewProjection[0][0]);

        // render quad
        glBindVertexArray(VAO);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, _models.size());
        glBindVertexArray(0);
    }
};

vec3 selectedColor = vec3(0, 1, 0);
bool leftMouseButtonPressed = false;
bool rightMouseButtonPressed = false;

class Grid
{
public:
    LineRenderer lines;
    QuadRenderer cells;

    Grid()
    {

        // glLineWidth(2);

        // draw horizontal lines of grid
        for (int j = 0; j <= GRID_HEIGHT; j++)
        {
            lines.addLine(vec3(0, j, 0), vec3(GRID_WIDTH, j, 0));
        }
        // draw vertical lines of grid
        for (int i = 0; i <= GRID_WIDTH; i++)
        {
            lines.addLine(vec3(i, 0, 0), vec3(i, GRID_HEIGHT, 0));
        };
    }

    void addCell(vec2 gridPos, vec3 color, bool updateImmediately = true)
    {

        // ignore mouse clicks outside the grid
        if (gridPos.x < 0 || gridPos.x > (GRID_WIDTH - 1) || gridPos.y < 0 || gridPos.y > (GRID_HEIGHT - 1))
        {
            return;
        }
        cells.addQuad(gridPos, color);
        if (updateImmediately)
        {
            cells.update();
        }
    }

    void removeCell(vec2 gridPos, bool updateImmediately = true)
    {

        // ignore mouse clicks outside the grid
        if (gridPos.x < 0 || gridPos.x > (GRID_WIDTH - 1) || gridPos.y < 0 || gridPos.y > (GRID_HEIGHT - 1))
        {
            return;
        }
        cells.remove(gridPos);
        if (updateImmediately)
        {
            cells.update();
        }
    }

    void draw()
    {

        cells.draw();
        lines.draw();
    }
};

Grid *grid;

int main()
{

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "grid", NULL, NULL);
    if (window == NULL)
    {
        cout << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    grid = new Grid();
    for (int i = 0; i < GRID_WIDTH; i++)
    {
        for (int j = 0; j < GRID_HEIGHT; j++)
        {
            // when setting up grid have updateImmediately=false
            // and batch update once at the end
            grid->addCell(vec2(i, j), vec3(1, 0, 0), false);
        }
    }
    // batch update
    grid->cells.update();

    // point camera at center of the grid, 15 units back from the grid
    cameraPos = vec3(GRID_WIDTH / 2, GRID_HEIGHT / 2, 15.0f);

    projection = perspective(radians(fov), ar, nearDist, farDist);
    glClearColor(1.0, 1.0, 1.0, 1.0);

    // set camera
    view = lookAt(cameraPos, cameraPos + cameraFront, vec3(0, 1, 0));
    grid->cells.setCamera(projection * view);
    grid->lines.setCamera(projection * view);

    while (!glfwWindowShouldClose(window))
    {

        // float currentFrame = glfwGetTime();
        // deltaTime = currentFrame - lastFrame;
        // lastFrame = currentFrame;

        // std::cout << "FPS: " << 1.0/(0.00000001+deltaTime) << std::endl;
        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT);

        grid->draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    delete grid;

    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // ray cast to find quad underneath mouse cursor

    if (leftMouseButtonPressed)
    {
        // cast from camera through mouse position
        vec3 rayWorld = rayCast(lastX, lastY, projection, view);
        // check for intersection with grid
        vec3 worldPos = rayPlaneIntersection(cameraPos, rayWorld, vec3(0, 0, 1), vec3(0, 0, 0));

        vec2 gridPos = vec2((int)worldPos.x, (int)worldPos.y);

        grid->addCell(gridPos, selectedColor);
    }
    if (rightMouseButtonPressed)
    {

        // cast from camera through mouse position
        vec3 rayWorld = rayCast(lastX, lastY, projection, view);
        // check for intersection with grid
        vec3 worldPos = rayPlaneIntersection(cameraPos, rayWorld, vec3(0, 0, 1), vec3(0, 0, 0));

        vec2 gridPos = vec2((int)worldPos.x, (int)worldPos.y);

        grid->removeCell(gridPos);
    }

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        selectedColor = vec3(1, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        selectedColor = vec3(0, 1, 0);
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        selectedColor = vec3(0, 0, 1);
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
        selectedColor = vec3(1, 1, 0);
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
        selectedColor = vec3(1, 0, 1);
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS)
        selectedColor = vec3(1, 1, 0);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    // switch to update less frequently when many squares on the screen
    if (cameraPos.z > 15.0f)
    {
        realTimeUpdating = false;
    }
    else
    {
        realTimeUpdating = true;
    }

    // scrolling moves camera closer and further from the grid
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    if (state == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        cameraPos -= scrollSpeed * vec3(xoffset / (float)SCR_WIDTH, yoffset / (float)SCR_WIDTH, 0);
        // update camera
        view = lookAt(cameraPos, cameraPos + cameraFront, vec3(0, 1, 0));
        grid->cells.setCamera(projection * view);
        grid->lines.setCamera(projection * view);
        grid->cells.calculateFrustum();

        if (realTimeUpdating)
        {
            grid->cells.update();
        }
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    scrollSpeed = cameraPos.z * 0.1;
    cameraPos += (float)yoffset * scrollSpeed * rayCast(lastX, lastY, projection, view);
    // update camera
    view = lookAt(cameraPos, cameraPos + cameraFront, vec3(0, 1, 0));
    grid->cells.setCamera(projection * view);
    grid->lines.setCamera(projection * view);
    grid->cells.calculateFrustum();
    grid->cells.update();
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        leftMouseButtonPressed = true;
    }
    else
    {
        leftMouseButtonPressed = false;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        rightMouseButtonPressed = true;
    }
    else
    {
        rightMouseButtonPressed = false;
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        if (!realTimeUpdating)
        {
            grid->cells.update();
        }
    }
}
