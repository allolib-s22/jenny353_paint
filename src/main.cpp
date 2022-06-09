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


   
#include <atomic>

#include "al/scene/al_DynamicScene.hpp"
#include "al/sound/al_Ambisonics.hpp"
#include "al/sound/al_Lbap.hpp"
#include "al/sound/al_Speaker.hpp"
#include "al/sound/al_StereoPanner.hpp"
#include "al/sound/al_Vbap.hpp"
#include "al/sphere/al_AlloSphereSpeakerLayout.hpp"
#include "Gamma/Oscillator.h"
#include "al/app/al_DistributedApp.hpp"
#include "al/graphics/al_Mesh.hpp"





using namespace std;

#define BLOCK_SIZE (512)

#define NPOINTS 1024

struct SharedState {
  uint16_t count;
  Vec3f position[NPOINTS];
  Color color[NPOINTS]; // *3 if all have the same alpha
};


using namespace al;

#define BLOCK_SIZE (512)

struct RayBrush : public DistributedAppWithState <SharedState> {

  Spatializer *spatializer{nullptr};

  double speedMult = 0.04f;
  double mElapsedTime = 0.0;

  ParameterVec3 srcpos{"srcPos", "", {0.0, 0.0, 0.0}};
  atomic<float> *mPeaks{nullptr};

  Speakers speakerLayout;

  int speakerType = 0;
  int spatializerType = 0;
  unsigned long counter = 0;  // overall sample counter

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;


  //std::vector<std::pair<Vec3f, Color>> objects;
  std::vector <Vec3f> pos; // keeps position for where to draw each sphere
  std::vector <Color> colorSpheres; //keeps track of color for each drawing stroke
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

void initSpeakers(int type = -1) {
    if (type < 0) {
      type = (speakerType + 1) % 3;
    }
    if (type == 0) {
      speakerLayout = AlloSphereSpeakerLayout();
    } else if (type == 1) {
      speakerLayout = SpeakerRingLayout<8>(0, 0, 5);
    } else if (type == 2) {
      speakerLayout = StereoSpeakerLayout(0, 30, 5);
    }
    speakerType = type;
    if (mPeaks) {
      free(mPeaks);
    }
    mPeaks = new atomic<float>[speakerLayout.size()];  // Not being freed
                                                       // in this example
  }

  void initSpatializer(int type) {
    if (spatializer) {
      delete spatializer;
    }
    spatializerType = type;
    if (type == 1) {
      spatializer = new Lbap(speakerLayout);
    } else if (type == 2) {
      spatializer = new Vbap(speakerLayout, speakerType == 0);
    } else if (type == 3) {
      spatializer = new AmbisonicsSpatializer(speakerLayout, 3, 1, 1);
    } else if (type == 4) {
      spatializer = new AmbisonicsSpatializer(speakerLayout, 3, 2, 1);
    } else if (type == 5) {
      spatializer = new AmbisonicsSpatializer(speakerLayout, 3, 3, 1);
    } else if (type == 6) {
      spatializer = new StereoPanner(speakerLayout);
    }
    spatializer->compile();
  }

  void onInit() override {
    audioIO().channelsBus(1);
    //addDodecahedron(mMesh);
    addSphere(mMesh, 0.2); 
    mMesh.generateNormals();
    initSpeakers(0);
    initSpatializer(1);

    //nav().pos(0, 3, 25);
    //nav().faceToward({0, 0, 0});
  }

  void onSound(AudioIOData &io) override {
    // Render signal to be panned
    
    while (io()) {
      float env = (22050 - (counter % 22050)) / 22050.0f;
      io.bus(0) = 0.5f * rnd::uniform() * env;
      ++counter;
    }
    //    // Spatialize
    spatializer->prepare(io);
    spatializer->renderBuffer(io, Pose(srcpos.get()), io.busBuffer(0),
                              io.framesPerBuffer());
    synthManager.render(io);  // Render audio
    spatializer->finalize(io);

    // Now compute RMS to display the signal level for each speaker
    for (size_t speaker = 0; speaker < speakerLayout.size(); speaker++) {
      float rms = 0;
      for (unsigned int i = 0; i < io.framesPerBuffer(); i++) {
        unsigned int deviceChannel = speakerLayout[speaker].deviceChannel;
        float sample = io.out(deviceChannel, i);
        rms += sample * sample;
      }
      rms = sqrt(rms / io.framesPerBuffer());
      mPeaks[speaker].store(rms);
      
    }
    
  }



  void onCreate() override {
    //for graphics
    nav().pos(0, 0, 100); //zoom in and out, higher z is out farther away
    light.pos(0, 0, 100); // where the light is set
    //addSphere(mMesh, 0.2); 
    //mMesh.generateNormals();

    //for sound

    // disable nav control mouse drag to look
    navControl().useMouse(false);

    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    imguiInit();
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().verbose(true);

    synthManager.synthSequencer().setDirectory("Sound-data");


  }



  void onAnimate(double dt) override {

        // Move source position
    mElapsedTime += dt;
    float tta = mElapsedTime * speedMult * 2.0f * M_PI;

    float x = 6.0f * cos(tta);
    float y = 5.0f * sin(2.8f * tta);
    float z = 6.0f * sin(tta);

    srcpos.set(Vec3d(x, y, z));
    // The GUI is prepared here
   
    //navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    //navControl().active(!isImguiUsingInput());  // not sure what this does

    //draw to shared state
    if(isPrimary()){
      state().count++;
      std::copy(pos.begin(), pos.end(), state().position);
      std::copy(colorSpheres.begin(), colorSpheres.end(), state().color);
    }
    
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
      pos.clear();
      colorSpheres.clear(); 
    }
    if (ImGui::Button("Undo")) {
        // undoes the most recent stroke from last on mouse down
        // erase the last elements from last_stroke_position to end:
        int getPrevStroke = start_stroke_positions.back();
        start_stroke_positions.pop_back();
        pos.erase(pos.begin() + getPrevStroke, pos.end());
        colorSpheres.erase(colorSpheres.begin() + getPrevStroke, colorSpheres.end());

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
    for (int i = 0; i < pos.size(); i++) {
      //std::cout<<"in onDraw, i = " << i << " pos: "<< pos[i] << "color = " <<colorSpheres[i].y<<std::endl;
      g.pushMatrix();
      g.translate(pos[i]);
      //g.color(1, .87, .5);
      g.color(colorSpheres[i]);
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


  bool onMouseDown(const Mouse &m) override {
    std::cout<<"Mouse Down"<< std::endl;
    // if mouse clicks on image gui, do not draw!!!
    if(isImguiUsingInput()){
      return true;
    }
    //start duration timer
    durationTimer.start();

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    //add a sphere to plane
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);

    start_stroke_positions.push_back(pos.size()); //set the last stroke positon to start from here
    pos.push_back(position);


    //trigger note on
    //set x and y coord on gui
    synthManager.voice()->setInternalParameterValue("x",m.x());
    synthManager.voice()->setInternalParameterValue("y",m.y());

    //mouse origin is upper left corner (0,0) -> (1200,800)
    //note mapping: lower left is lower freq, higher and up to right gives higher freq
    std::cout<<"m.x " << m.x() <<std::endl;
    std::cout<<"m.y " <<800-m.y() <<std::endl;
    midiNote = (m.x() + (800-m.y()))/50 + 65; //(range from 65-105)


    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    const float A4 = 220.f;
    if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
        synthManager.triggerOn(midiNote);
      }
    Color sphereColor = colorPicker;
    colorSpheres.push_back(sphereColor);
    
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
    //add a sphere to plane
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);

    pos.push_back(position);
    Color sphereColor = colorPicker;
    colorSpheres.push_back(sphereColor);
  
    return true;
  }

  bool onMouseUp(const Mouse &m) override {
    std::cout<<"Mouse Up Trigger off: "<< midiNote<< std::endl;
    durationTimer.stop();
    //trigger note off
    synthManager.triggerOff(midiNote);

    return true;
  }


  //for Loop Pedal Spacebar
   bool onKeyDown(const Keyboard &k) override {
     audioIO().stop();
    if (k.key() == 'i') {
      initSpeakers();
      initSpatializer(spatializerType);
    }

    if (k.key() == '1') {
      initSpatializer(1);
    } else if (k.key() == '2') {
      initSpatializer(2);
    } else if (k.key() == '3') {
      initSpatializer(3);
    } else if (k.key() == '4') {
      initSpatializer(4);
    } else if (k.key() == '5') {
      initSpatializer(5);
    } else if (k.key() == '6') {
      initSpatializer(6);
    }
    audioIO().start();

     if( k.key() == ' ' && recordLoop == false ){ //start recording loop
          //set recordLoop to true
          recordLoop = true;
          
          std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
          std::cout<<"start recording in file: sequenceName " << sequenceName <<std::endl;

          synthManager.synthRecorder().startRecord(sequenceName, true);
          
     }
      else if( k.key() == ' ' && recordLoop == true ){ //start playing recorded loop on repeat with sequencer 
          std::cout<<"stop recording " <<std::endl;
          synthManager.synthRecorder().stopRecord();
          
          const float A4 = 220.f;
          playLoop = true;
          recordLoop = false;
          std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
          std::cout<<"play sequence: " << sequenceName <<std::endl;
          sequenceFileNum++;
          
      } else if(k.shift() ){
        std::cout<<"shift pressed, end song " <<std::endl;
        //current sequence should finish and stop looping
        //synthManager.synthSequencer().stopSequence();
        //synthManager.synth().allNotesOff();
        playLoop = false;
  
      }
  
    return true;
  }

   void onExit() override {
    if (mPeaks) {
      free(mPeaks);
    }
  }

};
int main() {
  RayBrush app;
  // Set window size
  app.dimensions(1200, 800);
  AudioDevice::printAll();
  app.configureAudio(44100, BLOCK_SIZE, 60, 0);
  app.start();
  return 0;
}
