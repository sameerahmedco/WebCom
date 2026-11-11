#include "selection_capture_model.h"

#include <iostream>

namespace {
int checks=0, failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
}

int main(){
    using namespace webcom;
    const auto normal=NormalizeCaptureSelection(200,150,20,30,800,600);
    Check(normal && normal->left==20 && normal->top==30 && normal->right==200 && normal->bottom==150,
          "reverse drag normalizes");
    const auto clipped=NormalizeCaptureSelection(-30,-40,900,700,800,600);
    Check(clipped && clipped->left==0 && clipped->top==0 && clipped->right==800 && clipped->bottom==600,
          "selection clips to viewport");
    Check(!NormalizeCaptureSelection(10,10,12,200,800,600),"too-narrow selection rejected");
    Check(!NormalizeCaptureSelection(10,10,200,12,800,600),"too-short selection rejected");
    Check(!NormalizeCaptureSelection(0,0,20,20,0,600),"invalid viewport rejected");
    const CaptureSelectionRect source{100,50,500,350};
    const auto scaled=ScaleCaptureSelection(source,800,600,1600,1200);
    Check(scaled.left==200 && scaled.top==100 && scaled.right==1000 && scaled.bottom==700,
          "selection scales to capture bitmap");
    const auto scaled_fractional=ScaleCaptureSelection({1,1,3,3},4,4,10,10);
    Check(scaled_fractional.left==3 && scaled_fractional.top==3 && scaled_fractional.right==8 && scaled_fractional.bottom==8,
          "fractional bitmap scaling rounds predictably");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
