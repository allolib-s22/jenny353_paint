/*
Allocore 

Description:
Interact with paintbrush using ray intersection tests, and draw 3D spheres that are mapped sonically to 2D coordinates

*/
#include <stdlib.h> 

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/math/al_Ray.hpp"

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Domain.h"

#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_Window.hpp"
#include "al/system/al_Time.hpp"
#include "al/ui/al_PresetSequencer.hpp"


#include <vector>
#include "sound.hpp"

using namespace al;

struct RayBrush : App {
  ParameterServer paramServer;

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;

  // std::vector <Vec3f> pos; // keeps position for where to draw each sphere
  // std::vector <Color> colorSpheres; //keeps track of color for each drawing stroke

  std::vector<std::pair<Vec3f, Color>> objects;

  std::vector <int> start_stroke_positions; // keeps track of where all the stroke positions start for the undo function 
  Color colorPicker{1, 1, 1};
  bool move_with_mouse = false;
  int sequenceFileNum = 0;
  
  
  //for loop pedal
  bool recordLoop = false;
  bool playLoop = false;
  

  //variables for sound
  SynthGUIManager<Sound> synthManager{"Sound"};
  int midiNote = 0;
  float frequency = 0;
  Timer durationTimer;

  void onCreate() override {
    //for graphics
    nav().pos(0, 0, 100); //zoom in and out, higher z is out farther away
    light.pos(0, 0, 100); // where the light is set
    addSphere(mMesh, 0.2); 
    mMesh.generateNormals();

    //for sound

    // disable nav control mouse drag to look
    navControl().useMouse(false);

    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    imguiInit();
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().verbose(true);

    synthManager.synthSequencer().setDirectory("Sound-data");

    paramServer.listen();
    paramServer.registerOSCListener(&(oscDomain()->handler()));
    paramServer.print();
  }


  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData& io) override {
    synthManager.render(io);  // Render audio
  }

  void onAnimate(double dt) override {
    // The GUI is prepared here
   
    //navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    //navControl().active(!isImguiUsingInput());  // not sure what this does
    
    imguiBeginFrame();
    ImGui::Begin("my window");
    // Draw a window that contains the synth control panel
    synthManager.drawSynthControlPanel();
    
    // Edit 3 floats representing a color
    ImGui::ColorEdit3("Color", colorPicker.components);
    ImGui::Checkbox("Move With Mouse", &move_with_mouse);
    if (move_with_mouse) {
      navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    }
    //3 settings Loop pedal: record, play, off
    //ImGui::RadioButton("Record Loop", recordLoop); //Spacebar is pressed and loop recording is turned on 
    //ImGui::RadioButton("Play Loop", playLoop); //Spacebar is pressed while recordLoop is true, recordLoop becomes 
    // false and playLoop becomes true, looped recording is turned on
    ImGui::Checkbox("Record Loop", &recordLoop);
    ImGui::Checkbox("Play Loop", &playLoop);


    if (ImGui::Button("Clear Drawing")) {
      // Buttons return true when clicked
      // clears the screen

      objects.clear();
    }
    if (ImGui::Button("Undo")) {
        // undoes the most recent stroke from last on mouse down
        // erase the last elements from last_stroke_position to end:
        int getPrevStroke = start_stroke_positions.back();
        start_stroke_positions.pop_back();
        objects.erase(objects.begin() + getPrevStroke, objects.end());
    }
    ImGui::End();
    imguiEndFrame();
  }


  virtual void onDraw(Graphics &g) override {
    g.clear(0);
    gl::depthTesting(true);
    g.lighting(true);
    //std::cout<<"in onDraw "<< " pos[0]: "<< pos[0] <<std::endl;
    //draw and color spheres 
    for (int i = 0; i < objects.size(); i++) {
      std::pair<Vec3d, Color> object = objects.at(i);
      
      g.pushMatrix();
      g.translate(object.first);
      g.color(object.second);
      g.draw(mMesh);
      g.popMatrix();
    }
    // Render the synth's graphics
    synthManager.render(g);
    // GUI is drawn here
    imguiDraw();
    //change z coord with gui
    nav().pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); //zoom in and out, higher z is out farther away
    light.pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); // where the light is set

    if(!synthManager.synthSequencer().playing() && playLoop == true ){ // keeps looping if playLoop is on
        std::string sequenceName = "sound" + std::to_string(sequenceFileNum-1); //loop prev file 
        std::cout<<"play sequence: " << sequenceName <<std::endl;
        synthManager.synthSequencer().playSequence(sequenceName + ".synthSequence");
    }
  }

  Vec3d unproject(Vec3d screenPos) {
    auto &g = graphics();
    auto mvp = g.projMatrix() * g.viewMatrix() * g.modelMatrix();
    Matrix4d invprojview = Matrix4d::inverse(mvp);
    Vec4d worldPos4 = invprojview.transform(screenPos);
    return worldPos4.sub<3>(0) / worldPos4.w;
  }

  bool addToScreen(Vec3d position, Color* color = NULL) {
    Color sphereColor = color == NULL ? colorPicker : *color;

    //add a sphere to plane
    objects.push_back(std::pair<Vec3d, Color>(position, sphereColor));

    return true;
  }
  
  void noteStart(Vec3d position, Color* color = NULL) {
    //start duration timer
    durationTimer.start();

    start_stroke_positions.push_back(objects.size()); //set the last stroke positon to start from here

    //trigger note on
    //set x and y coord on gui
    synthManager.voice()->setInternalParameterValue("x", position.x);
    synthManager.voice()->setInternalParameterValue("y", position.y);

    //mouse origin is upper left corner (0,0) -> (1200,800)
    //note mapping: lower left is lower freq, higher and up to right gives higher freq
    std::cout<<"m.x " << position.x <<std::endl;
    std::cout<<"m.y " << 800 - position.y <<std::endl;
    midiNote = (position.x + (800 - position.y))/50 + 65; //(range from 65-105)

    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    const float A4 = 220.f;
    if (midiNote > 0) {
      synthManager.voice()->setInternalParameterValue(
          "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
      synthManager.triggerOn(midiNote);
    }
  }

  void noteEnd() {
    std::cout<<"Mouse Up Trigger off: "<< midiNote << std::endl;
    durationTimer.stop();
    //trigger note off
    synthManager.triggerOff(midiNote);
  }

  bool onMouseDown(const Mouse &m) override {
    std::cout<<"Mouse Down"<< std::endl;
    // if mouse clicks on image gui, do not draw!!!
    if(isImguiUsingInput()){
      return true;
    }

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    //add a sphere to plane
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);

    addToScreen(position);
    noteStart(position);
    
    return true;
  }

  bool onMouseDrag(const Mouse &m) override {
    // if mouse drags on image gui, do not draw!!!
    if(isImguiUsingInput()){
      return true;
    }

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);

    return addToScreen(worldPos);
  }

  bool onMouseUp(const Mouse &m) override {
    noteEnd();
    return true;
  }

  //for Loop Pedal Spacebar
   bool onKeyDown(const Keyboard &k) override {
     if (k.key() == ' ' && recordLoop == false ) { //start recording loop
      //set recordLoop to true
      recordLoop = true;
      
      std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
      std::cout<<"start recording in file: sequenceName " << sequenceName <<std::endl;

      synthManager.synthRecorder().startRecord(sequenceName, true);
    } else if(k.key() == ' ' && recordLoop == true ) { //start playing recorded loop on repeat with sequencer 
      std::cout<<"stop recording " <<std::endl;
      synthManager.synthRecorder().stopRecord();
      
      const float A4 = 220.f;
      playLoop = true;
      recordLoop = false;
      std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
      std::cout<<"play sequence: " << sequenceName <<std::endl;
      sequenceFileNum++;
    } else if (k.shift()) {
      std::cout<<"shift pressed, end song " <<std::endl;
      //current sequence should finish and stop looping
      //synthManager.synthSequencer().stopSequence();
      //synthManager.synth().allNotesOff();
      playLoop = false;
    }

    return true;
  }

  // This gets called whenever we receive a packet
  void onMessage(osc::Message& m) override {
    // m.print();

    // Check that the address and tags match what we expect
    if (m.addressPattern().find("/rotation") != std::string::npos && m.typeTags() == "iiiiiii") {
      // Extract the data out of the packet
      int xInt, yInt, zInt, cr, cg, cb, touch;
      m >> xInt;
      m >> yInt;
      m >> zInt;
      m >> cr;
      m >> cg;
      m >> cb;
      m >> touch;

      Vec3d position;

      position.x = xInt / 1000.0;
      if (position.x < 180) {
        position.x *= -1;
      } else {
        position.x = 360 - position.x;
      }

      position.y = yInt / 1000.0;
      position.z = zInt / 1000.0;

      Color color(cr / 256.0f, cg / 256.0f, cb / 256.0f);

      addToScreen(position, &color);

      if (touch == 1) {
        noteStart(position, &color);
      } else if (touch == -1) {
        noteEnd();
      }
    }
  }
};

int main() {
  RayBrush app;
  // Set window size
  app.dimensions(1200, 800);
  app.configureAudio(48000., 512, 2, 0);
  app.start();
  return 0;
}
