#include "capture_model.h"

#include <iostream>
#include <vector>

namespace {
unsigned checks=0, failures=0;
void Check(bool value, const char* label) { ++checks; if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';} }
}

int main(){
    using webcom::DecodeCaptureScreenshotResult;
    const auto one=DecodeCaptureScreenshotResult(L"{\"data\":\"AQIDBA==\"}");
    Check(one && *one==std::vector<std::uint8_t>({1,2,3,4}),"base64 screenshot data decodes");
    const auto spaced=DecodeCaptureScreenshotResult(L"{ \"other\":1, \"data\" : \"aGVsbG8=\" }");
    Check(spaced && std::string(spaced->begin(),spaced->end())=="hello","data field can follow another field");
    Check(!DecodeCaptureScreenshotResult(L"{\"data\":\"bad?\"}"),"invalid base64 rejected");
    Check(!DecodeCaptureScreenshotResult(L"{\"nope\":\"AQID\"}"),"missing data field rejected");
    Check(!DecodeCaptureScreenshotResult(L"{\"data\":\"AQID\"}",2),"decoded byte cap enforced");
    Check(!DecodeCaptureScreenshotResult(L"{\"data\":\"AQ=Z\"}"),"invalid padding rejected");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
