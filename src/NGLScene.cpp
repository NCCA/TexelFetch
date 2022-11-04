#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Transformation.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOFactory.h>
#include <ngl/SimpleVAO.h>
#include <ngl/ShaderLib.h>
#include <ngl/Vec2.h>
#include <ngl/Random.h>

NGLScene::NGLScene()
{
  setTitle("Qt5 Simple NGL Demo");
}

NGLScene::~NGLScene()
{
  std::cout << "Shutting down NGL, removing VAO's and Shaders\n";
}

void NGLScene::resizeGL(int _w, int _h)
{
  m_project = ngl::perspective(45.0f, static_cast<float>(_w) / _h, 0.05f, 350.0f);
  m_win.width = static_cast<int>(_w * devicePixelRatio());
  m_win.height = static_cast<int>(_h * devicePixelRatio());
}

void NGLScene::initializeGL()
{
  // we need to initialise the NGL lib which will load all of the OpenGL functions, this must
  // be done once we have a valid GL context but before we call any GL commands. If we dont do
  // this everything will crash
  ngl::NGLInit::initialize();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f); // Grey Background
  // enable depth testing for drawing

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0, 1, 4);
  ngl::Vec3 to(0, 0, 0);
  ngl::Vec3 up(0, 1, 0);

  m_view = ngl::lookAt(from, to, up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_project = ngl::perspective(45, 1024.0f / 720.0f, 0.01f, 150);

  // now to load the shader and set the values
  // grab an instance of shader manager
  ngl::ShaderLib::use("nglColourShader");
  ngl::ShaderLib::setUniform("Colour", 1.0f, 1.0f, 1.0f, 1.0f);
  buildVAO();
  buildTextureBuffer();
  glViewport(0, 0, width(), height());
  ngl::ShaderLib::loadShader("TexelShader", "shaders/VertexShader.glsl", "shaders/FragmentShader.glsl");
  startTimer(20);
}

void NGLScene::buildVAO()
{
  std::vector<ngl::Vec2> xz;

  for (float z = -m_gridDimension; z < m_gridDimension; z += m_gridStep)
  {
    for (float x = -m_gridDimension; x < m_gridDimension; x += m_gridStep)
    {
      xz.push_back({x, z});
    }
  }
  // create a vao as a series of GL_TRIANGLES
  m_vao = ngl::VAOFactory::createVAO(ngl::simpleVAO, GL_POINTS);
  m_vao->bind();

  // in this case we are going to set our data as the vertices above
  m_vao->setData(ngl::SimpleVAO::VertexData(xz.size() * sizeof(ngl::Vec2), xz[0].m_x));
  // now we set the attribute pointer to be 0 (as this matches vertIn in our shader)

  m_vao->setVertexAttributePointer(0, 2, GL_FLOAT, 0, 0);

  m_vao->setNumIndices(xz.size());

  // now unbind
  m_vao->unbind();
  m_gridSize = xz.size();
}

void NGLScene::buildTextureBuffer()
{
  // We are going to build a simple array of float values for the height
  std::vector<float> y;

  for (float z = -m_gridDimension; z < m_gridDimension; z += m_gridStep)
  {
    for (float x = -m_gridDimension; x < m_gridDimension; x += m_gridStep)
    {
      y.push_back(sin((x)));
    }
  }
  // now generate a buffer and copy this data to it
  // we will update this every frame with new values.
  glGenBuffers(1, &m_yposID);

  glBindBuffer(GL_TEXTURE_BUFFER, m_yposID);
  glBufferData(GL_TEXTURE_BUFFER, y.size() * sizeof(float), &y[0], GL_STATIC_DRAW);
  // This buffer is now going to be associated with a texture
  // this will be read in the shader and the index will be from the vertexID
  glGenTextures(1, &m_tboID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_tboID);

  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, m_yposID);
}
void NGLScene::updateTextureBuffer()
{
  // add this offset each frame to make the sin travel
  static float offset = 0.0f;
  // we could resize this rather than push back forspeed.
  std::vector<float> y;

  for (float z = -m_gridDimension; z < m_gridDimension; z += m_gridStep)
  {
    for (float x = -m_gridDimension; x < m_gridDimension; x += m_gridStep)
    {
      y.push_back(sin((x + offset)) + cos(x - offset));
    }
  }
  // update this buffer by copying the data to it
  glBindBuffer(GL_TEXTURE_BUFFER, m_yposID);
  glBufferData(GL_TEXTURE_BUFFER, y.size() * sizeof(float), &y[0], GL_STATIC_DRAW);
  // update by a small amount to make the sin travel
  offset += 0.01f;
}

void NGLScene::paintGL()
{
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // Rotation based on the mouse position for our global transform
  ngl::Transformation trans;
  // Rotation based on the mouse position for our global transform
  ngl::Mat4 rotX = ngl::Mat4::rotateX(m_win.spinXFace);
  ngl::Mat4 rotY = ngl::Mat4::rotateY(m_win.spinYFace);
  // multiply the rotations
  m_mouseGlobalTX = rotX * rotY;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;
  ngl::ShaderLib::use("TexelShader");

  ngl::Mat4 MVP;
  MVP = m_project * m_view * m_mouseGlobalTX;

  ngl::ShaderLib::setUniform("MVP", MVP);

  glPointSize(4);
  m_vao->bind();
  ngl::ShaderLib::setUniform("MVP", MVP);
  m_vao->draw();
  m_vao->unbind();
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseMoveEvent(QMouseEvent *_event)
{
// note the method buttons() is the button state when event was called
// that is different from button() which is used to check which button was
// pressed when the mousePress/Release event is generated
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  auto position = _event->position();
#else
  auto position = _event->pos();
#endif
  if (m_win.rotate && _event->buttons() == Qt::LeftButton)
  {
    int diffx = position.x() - m_win.origX;
    int diffy = position.y() - m_win.origY;
    m_win.spinXFace += static_cast<int>(0.5f * diffy);
    m_win.spinYFace += static_cast<int>(0.5f * diffx);
    m_win.origX = position.x();
    m_win.origY = position.y();
    update();
  }
  // right mouse translate code
  else if (m_win.translate && _event->buttons() == Qt::RightButton)
  {
    int diffX = static_cast<int>(position.x() - m_win.origXPos);
    int diffY = static_cast<int>(position.y() - m_win.origYPos);
    m_win.origXPos = position.x();
    m_win.origYPos = position.y();
    m_modelPos.m_x += INCREMENT * diffX;
    m_modelPos.m_y -= INCREMENT * diffY;
    update();
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mousePressEvent(QMouseEvent *_event)
{
// that method is called when the mouse button is pressed in this case we
// store the value where the maouse was clicked (x,y) and set the Rotate flag to true
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  auto position = _event->position();
#else
  auto position = _event->pos();
#endif
  if (_event->button() == Qt::LeftButton)
  {
    m_win.origX = position.x();
    m_win.origY = position.y();
    m_win.rotate = true;
  }
  // right mouse translate mode
  else if (_event->button() == Qt::RightButton)
  {
    m_win.origXPos = position.x();
    m_win.origYPos = position.y();
    m_win.translate = true;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseReleaseEvent(QMouseEvent *_event)
{
  // that event is called when the mouse button is released
  // we then set Rotate to false
  if (_event->button() == Qt::LeftButton)
  {
    m_win.rotate = false;
  }
  // right mouse translate mode
  if (_event->button() == Qt::RightButton)
  {
    m_win.translate = false;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::wheelEvent(QWheelEvent *_event)
{

  // check the diff of the wheel position (0 means no change)
  if (_event->angleDelta().x() > 0)
  {
    m_modelPos.m_z += ZOOM;
  }
  else if (_event->angleDelta().x() < 0)
  {
    m_modelPos.m_z -= ZOOM;
  }
  update();
}
//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quite
  case Qt::Key_Escape:
    QGuiApplication::exit(EXIT_SUCCESS);
    break;
  // turn on wirframe rendering
  case Qt::Key_W:
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    break;
  // turn off wire frame
  case Qt::Key_S:
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    break;
  // show full screen
  case Qt::Key_F:
    showFullScreen();
    break;
  // show windowed
  case Qt::Key_N:
    showNormal();
    break;
  default:
    break;
  }
  // finally update the GLWindow and re-draw
  // if (isExposed())
  update();
}

void NGLScene::timerEvent(QTimerEvent *)
{
  updateTextureBuffer();
  update();
}
