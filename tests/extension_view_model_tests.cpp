#include "extension_view_model.h"
#include <iostream>

namespace { unsigned checks=0, failures=0; void Check(bool v,const char* label){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<label<<'\n';}} }

int main(){
    using namespace webcom;
    ExtensionViewState healthy{true,false,true,true,true,false,true};
    Check(!ExtensionNeedsAttention(healthy),"healthy recorded source has no attention flag");
    Check(ExtensionMatchesFilter(healthy,ExtensionViewFilter::Enabled),"enabled filter matches enabled item");
    Check(!ExtensionMatchesFilter(healthy,ExtensionViewFilter::Disabled),"disabled filter excludes enabled item");

    auto changed=healthy; changed.source_changed=true;
    Check(ExtensionNeedsAttention(changed),"changed source needs attention");
    auto missing=healthy; missing.source_recorded=false;
    Check(ExtensionNeedsAttention(missing),"unrecorded third-party source needs attention");
    auto builtin=missing; builtin.built_in=true;
    Check(!ExtensionNeedsAttention(builtin),"built-in extension does not need source mapping");
    auto baseline=healthy; baseline.source_baseline_known=false;
    Check(ExtensionNeedsAttention(baseline),"unknown source baseline needs attention");
    auto unavailable=healthy; unavailable.source_snapshot_available=false;
    Check(ExtensionNeedsAttention(unavailable),"unavailable source scan needs attention");

    Check(ExtensionMatchesText(L"Privacy Helper",L"abc123",L"C:/extensions/privacy",L"privacy"),"text filter matches name/path case-insensitively");
    Check(ExtensionMatchesText(L"Other",L"ABC123",L"C:/src",L"abc"),"text filter matches extension id case-insensitively");
    Check(!ExtensionMatchesText(L"Other",L"xyz",L"C:/src",L"privacy"),"text filter excludes unrelated item");

    ExtensionViewCounts counts;
    AddExtensionViewCount(counts,healthy);
    auto disabled=healthy; disabled.enabled=false; AddExtensionViewCount(counts,disabled);
    AddExtensionViewCount(counts,changed);
    Check(counts.all==3,"counts all");
    Check(counts.enabled==2&&counts.disabled==1,"counts enabled and disabled");
    Check(counts.attention==1,"counts attention");
    Check(ExtensionMatchesFilter(changed,ExtensionViewFilter::NeedsAttention),"attention filter matches changed source");
    Check(ExtensionMatchesFilter(healthy,ExtensionViewFilter::All),"all filter matches everything");

    std::cout<<checks<<" extension view checks, "<<failures<<" failures\n";
    return failures?1:0;
}
