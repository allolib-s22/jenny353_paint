/*
Allocore 

Description:
Uses mouse as a paintbrush to draw 3D spheres that are mapped sonically using 2D coordinates.


*/
#include <stdlib.h> 
#include <iostream>
#include <fstream>


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
#include "subsound.hpp"


using namespace al;


struct MusicBrush : App {

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;

  std::vector <std::pair<Vec3f, float>> pos; // keeps position for where to draw each sphere, and its time elapsed from on mouseDown
  std::vector <int> start_stroke_positions; // keeps track of where all the stroke positions start for the undo function 
  Color colorPicker{1.f, 1.f, 1.f, 1.f};
  bool move_with_mouse = false;
  int sequenceFileNum = 0;
  
  //
  
  //for loop pedal
  bool recordLoop = false;
  bool playLoop = false;

  //variables for sound
  SynthGUIManager<Sub> synthManager{"Sub"};
  int midiNote = 0;
  float frequency = 0;
  Clock clock1;
  Clock clock2;

  float startStrokeTime = 0.f;
  float startLoopTime = 0.f;
  float startLoopPos = 0.f;
  bool setFirst = false;
  int getMostRecentStrokeStart;
  

  void onCreate() override {
    clock1.useRT();
    clock2.useRT();
    // std::cout<< "clock.now " <<clock.now() <<std::endl;
    // std::cout<< "clock.rt " <<clock.rt() <<std::endl;
    // std::cout<< "clock.dt " <<clock.dt() <<std::endl;
    // std::cout<< "clock.frame " <<clock.frame() <<std::endl;
    // std::cout<< "clock.fps " <<clock.fps() <<std::endl;
    // std::cout<< "clock.update() " <<clock.update() <<std::endl;
    // std::cout<< "clock.now " <<clock.now() <<std::endl;
    
    //for graphics
    nav().pos(0, 0, 100); 
    light.pos(0, 0, 100); // where the light is set

    addSphere(mMesh, 0.2);
    mMesh.generateNormals();
    //mMesh.smooth();

    // disable nav control mouse drag to look
    navControl().useMouse(false);

    


    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    imguiInit();
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().verbose(true);

    synthManager.synthSequencer().setDirectory("Sub-data");
  
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
    
    // Edit 4 floats representing a color , r g b a
    ImGui::ColorEdit4("Color", colorPicker.components);
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
    }
    if (ImGui::Button("Undo")) {
        // undoes the most recent stroke from last on mouse down
        // erase the last elements from last_stroke_position to end:
        int getPrevStroke = start_stroke_positions.back();
        start_stroke_positions.pop_back();
        pos.erase(pos.begin() + getPrevStroke, pos.end());

    }
    ImGui::End();
    imguiEndFrame();
  }


  virtual void onDraw(Graphics &g) override { 

    g.clear();
    gl::depthTesting(true);
    g.lighting(true);
    //g.blending(true);
    
    //draw and color spheres 
  // if(playLoop && !recordLoop ){
  //     //if we are drawing while replaying the loops
  //     //need to draw the current on mouse drags that are not captured in the while below
  //     getMostRecentStrokeStart = start_stroke_positions.back();
  //     int j = getMostRecentStrokeStart+1;
  //       while(j < pos.size()-1){
  //         g.pushMatrix();
  //         g.translate(pos[j].first);
  //         g.color(colorPicker);
  //         g.draw(mMesh);
  //         g.popMatrix();
  //         j++;
  //       }
  //       getMostRecentStrokeStart = j+1;
        
  //   }



    //load sequence, for each active voice, use getInternalParameterValue(startPos)
        //for each active voice,  make this the start pos and draw the strokes that start there
    auto *voices = synthManager.synthSequencer().synth().getActiveVoices();
  
  while (voices) {

    int startPos = voices->getInternalParameterValue("startPos");
    
    if(!setFirst){
      startLoopPos = startPos;
      setFirst = true;
    }
    
    float red = voices->getInternalParameterValue("colorR");
    float green = voices->getInternalParameterValue("colorG");
    float blue = voices->getInternalParameterValue("colorB");
    float alpha = voices->getInternalParameterValue("colorA");
    Color colorSphere{red, green,blue,alpha};

    int i = startPos;
    //std::cout<<"pos.size()"<<pos.size()<<std::endl; 
    //only draw that dots that are within their duration
      clock2.update();
      float currentTime = clock2.now();
      //std::cout<<"On draw 1:  currenttime"<< currentTime <<std::endl;
          

      while(i < pos.size() && !(pos[i].first.x == 0 && pos[i].first.y == 0  && pos[i].first.z == 0) && ( pos[i].second - pos[startLoopPos].second <= currentTime - startLoopTime)){
        //std::cout<<"in onDraw, i = " << i << " posx: "<< pos[i].x <<  " posy: "<< pos[i].y <<  " posz: "<< pos[i].z <<" color = " << colorSphere.r<<std::endl;
        g.pushMatrix();
        g.translate(pos[i].first);
        g.color(colorSphere);
        g.draw(mMesh);
        g.popMatrix();
        i++;
        clock2.update();
        currentTime = clock2.now();
        //std::cout<<"On draw 2:  currenttime"<< currentTime <<std::endl;
        //std::cout<<"On draw 2:  currentTime - startLoopTime"<< currentTime - startLoopTime <<std::endl;
        // std::cout<< "startLooptime : " << startLoopTime <<std::endl;
        // std::cout<< "currentTime : " <<currentTime <<std::endl;
        // std::cout<< "currentTime - startLoopTime: " <<currentTime - startLoopTime <<std::endl;
        // std::cout<< "pos[i].second : " << pos[i].second <<std::endl;
        // std::cout<< "pos[startPos].second : " <<pos[startPos].second <<std::endl;
         //std::cout<< "pos[i].second - pos[startPos].second: " <<pos[i].second - pos[startPos].second<<std::endl;
         //std::cout<< " pos[startPos].second: " <<pos[startPos].second<<std::endl;
        
    }
            voices = voices->next;
    }
    
    //
    // Render the synth's graphics
    synthManager.render(g);
    // GUI is drawn here
    imguiDraw();
    //change z coord with gui
    //std::cout<<"posZ: " << synthManager.voice()->getInternalParameterValue("z") <<std::endl;
    nav().pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); //zoom in and out, higher z is out farther away
    

    if(!synthManager.synthSequencer().playing() && playLoop == true ){ // keeps looping if playLoop is on
        std::string sequenceName = "sound" + std::to_string(sequenceFileNum-1); //loop prev file 
        std::cout<<"play sequence: " << sequenceName <<std::endl;
        synthManager.synthSequencer().playSequence(sequenceName + ".synthSequence"); 
        clock2.update();
        startLoopTime = clock2.now();
        //std::cout<<"Start Loop:  startLoopTime"<< startLoopTime <<std::endl;
        setFirst = false;
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
    // if mouse clicks on image gui, do not draw!!!
     if(isImguiUsingInput()){
      return true;
    }
    std::cout<<"Mouse Down"<< std::endl;
     

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);

    start_stroke_positions.push_back(pos.size()); //set the last stroke positon to start from here

    //pos.push_back(position); first sphere is drawn on onProcess

    Color sphereColor = colorPicker;

    synthManager.voice()->setInternalParameterValue("posX",worldPos.x);
    synthManager.voice()->setInternalParameterValue("posY",worldPos.y);
    synthManager.voice()->setInternalParameterValue("posZ",worldPos.z);
    synthManager.voice()->setInternalParameterValue("colorR",sphereColor.r);
    synthManager.voice()->setInternalParameterValue("colorG",sphereColor.g);
    synthManager.voice()->setInternalParameterValue("colorB",sphereColor.b);
    synthManager.voice()->setInternalParameterValue("colorA",sphereColor.a);
    synthManager.voice()->setInternalParameterValue("startPos",pos.size());


    // use color to change timbre
    std::cout<<"sphereColor.a = "<< sphereColor.a <<std::endl; 
    float timbre = (sphereColor.a); 
    std::cout<<"Timbre = "<< timbre <<std::endl;   
    updateTimbre(timbre);


    //trigger note on

    
    //mouse origin is upper left corner (0,0) -> (1200,800)
    //note mapping: lower left is lower freq, higher and up to right gives higher freq
    midiNote = (m.x() + (800-m.y()))/50 + 65; //(range from 65-105)


    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    const float A4 = 220.f;
    if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
        synthManager.triggerOn(midiNote);
      }

    
    return true;
  }

  void updateTimbre(float timbre){
      synthManager.voice()->setInternalParameterValue("cf1", 
        synthManager.voice()->getInternalParameterValue("cf1") * timbre);
      synthManager.voice()->setInternalParameterValue("cf2", 
        synthManager.voice()->getInternalParameterValue("cf2") * timbre);
      synthManager.voice()->setInternalParameterValue("cf3", 
        synthManager.voice()->getInternalParameterValue("cf4") * timbre);
      synthManager.voice()->setInternalParameterValue("bw1", 
        synthManager.voice()->getInternalParameterValue("bw1") * timbre);
      synthManager.voice()->setInternalParameterValue("bw2", 
        synthManager.voice()->getInternalParameterValue("bw2") * timbre);

  }

  bool onMouseDrag(const Mouse &m) override {
    //std::cout<<"Mouse Drag " <<std::endl;
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

    clock1.update();
    float timeElapsedFromStart = clock1.now() - startStrokeTime; 
    std::cout<<"on Mouse Drag:  timeElapsedFromstart"<< timeElapsedFromStart <<std::endl;
          
    
    pos.push_back(std::pair<Vec3d, al_sec>(position, timeElapsedFromStart));
    Color sphereColor = colorPicker;
    

    synthManager.voice()->setInternalParameterValue("posX",worldPos.x);
    synthManager.voice()->setInternalParameterValue("posY",worldPos.y);
    synthManager.voice()->setInternalParameterValue("posZ",worldPos.z);
    synthManager.voice()->setInternalParameterValue("colorR",sphereColor.r);
    synthManager.voice()->setInternalParameterValue("colorG",sphereColor.g);
    synthManager.voice()->setInternalParameterValue("colorB",sphereColor.b);
    synthManager.voice()->setInternalParameterValue("colorA",sphereColor.a);

    return true;
  }

  bool onMouseUp(const Mouse &m) override {
    if(isImguiUsingInput()){
      return true;
    }

    std::cout<<"Mouse Up Trigger off: "<< midiNote<< std::endl;
    

    //trigger note off
    synthManager.triggerOff(midiNote);
    
    //change timbre back
    Color sphereColor = colorPicker;
    float original_timbre = 1.0/(sphereColor.a); 
    updateTimbre(original_timbre);

    Vec3f nullVal = Vec3f{0,0,0};
    
    pos.push_back(std::pair<Vec3d, al_sec>(nullVal, 0)); //to mark the end of the stroke drawing 

    return true;
  }


  //for Loop Pedal Spacebar
   bool onKeyDown(const Keyboard &k) override {
     if( k.key() == ' ' && recordLoop == false ){ //start recording loop
          //set recordLoop to true
          recordLoop = true;
          clock1.update();
          startStrokeTime = clock1.now();
          std::cout<<"Start recording:  startStrokeTime"<< startStrokeTime <<std::endl;
          
          std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
          std::cout<<"start recording in file: sequenceName " << sequenceName <<std::endl;

          synthManager.synthRecorder().startRecord(sequenceName, true);
          setFirst = false;
          
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

};
int main() {
  MusicBrush app;
  // Set window size
  app.dimensions(1200, 800);
  app.configureAudio(48000., 512, 2, 0);
  app.start();
  return 0;
}
