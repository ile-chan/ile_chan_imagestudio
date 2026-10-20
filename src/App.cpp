#include "ImageEngine.h"
#include <windowsx.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace wky;
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT Finished=WM_APP+1;
enum { Open=101,Save,Undo,Reset,Apply,Category,Algorithms,Zoom,Help,Demo,Param0=150,Preset=170,KernelSize,KernelEdit,Normalize,Flip,Color,Divisor,Bias,BorderMode,Info };
const wchar_t* categories[]={L"灰度变换与增强",L"平滑滤波与去噪",L"边缘检测与锐化",L"阈值与聚类分割",L"轮廓与直线检测",L"直方图分析",L"自定义卷积核"};
COLORREF Paper=RGB(247,248,246),Ink=RGB(31,47,44),Muted=RGB(88,105,99),Accent=RGB(13,106,87),Stage=RGB(33,38,40),Line=RGB(215,223,218);
struct Snapshot {Image image;std::wstring name,details;};
struct JobResult {int kind=0;Result result;std::wstring path,route,name;double milliseconds=0;};
enum class PendingAction {None,OpenDialog,OpenPath,Demo,Close};
struct State {
    HWND window=nullptr,category=nullptr,list=nullptr,explanation=nullptr,info=nullptr,zoom=nullptr;
    HWND labels[4]{},edits[4]{},preset=nullptr,kernelSize=nullptr,kernelEdit=nullptr,normalize=nullptr,flip=nullptr,color=nullptr,divisor=nullptr,bias=nullptr,border=nullptr;
    HWND divLabel=nullptr,biasLabel=nullptr,borderLabel=nullptr;
    HFONT body=nullptr,smallFont=nullptr,title=nullptr,matrix=nullptr;
    HBRUSH paper=CreateSolidBrush(Paper),white=CreateSolidBrush(RGB(255,255,255));
    int dpi=96,width=1360,height=880;bool busy=false,dirty=false,dragging=false,test=false,grayscale=true;
    Image original,current;std::vector<BYTE> originalDib,currentDib;
    Statistics statistics;std::vector<Snapshot> history;
    std::wstring path,operation=L"原图",details,status=L"打开图像开始处理，或载入演示图。";
    std::thread worker;std::atomic<bool> cancelled{false};
    std::vector<Op> visible;Op selected=Op::Gray;
    double zoomFactor=0;POINT pan{},dragStart{},panStart{};
    std::wstring testDirectory;int testPhase=0,testFailures=0;
    PendingAction pending=PendingAction::None;std::wstring pendingPath,lastError;bool expectedTestError=false;
    std::vector<BYTE> testBefore;
    int S(int n)const{return MulDiv(n,dpi,96);}
} app;
void RunPendingAction();
std::string Utf8Message(const std::wstring& text){int n=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);std::string result(n,'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),n,nullptr,nullptr);return result;}

std::wstring Text(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(size_t(n)+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
HWND Control(const wchar_t* type,const wchar_t* text,DWORD style,int id,HWND parent=nullptr){
    HWND h=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,parent?parent:app.window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(app.body),TRUE);return h;
}
void Place(HWND h,int x,int y,int width,int height){MoveWindow(h,app.S(x),app.S(y),app.S(width),app.S(height),TRUE);}
void FontSetup(){
    for(auto font:{app.body,app.smallFont,app.title,app.matrix})if(font)DeleteObject(font);
    auto create=[](int size,int weight,const wchar_t* name){return CreateFontW(-app.S(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,name);};
    app.body=create(14,FW_NORMAL,L"Microsoft YaHei UI");app.smallFont=create(12,FW_NORMAL,L"Microsoft YaHei UI");app.title=create(21,FW_SEMIBOLD,L"Microsoft YaHei UI");app.matrix=create(15,FW_NORMAL,L"Consolas");
    if(app.window)EnumChildWindows(app.window,[](HWND h,LPARAM){SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(app.body),TRUE);return TRUE;},0);
    if(app.kernelEdit)SendMessageW(app.kernelEdit,WM_SETFONT,reinterpret_cast<WPARAM>(app.matrix),TRUE);
}
void EnableActions(){
    for(int id:{Open,Save,Undo,Reset,Apply,Category,Algorithms,Preset,KernelSize,KernelEdit,Normalize,Flip,Color,Divisor,Bias,BorderMode,Demo}){
        HWND h=GetDlgItem(app.window,id);bool allowed=!app.busy;
        if(id==Save||id==Reset||id==Apply)allowed=allowed&&app.current.valid();
        if(id==Undo)allowed=allowed&&!app.history.empty();EnableWindow(h,allowed);
    }
    for(auto edit:app.edits)EnableWindow(edit,!app.busy);
    EnableWindow(app.divisor,!app.busy&&SendMessageW(app.normalize,BM_GETCHECK,0,0)!=BST_CHECKED);
    SetWindowTextW(GetDlgItem(app.window,Apply),app.busy?L"处理中…":L"应用到当前图像   F5");InvalidateRect(app.window,nullptr,FALSE);
}
void RebuildDib(const Image& image,std::vector<BYTE>& dib){dib.resize(size_t(image.width)*image.height*4);for(size_t i=0;i<image.bgr.size()/3;++i){dib[i*4]=image.bgr[i*3];dib[i*4+1]=image.bgr[i*3+1];dib[i*4+2]=image.bgr[i*3+2];dib[i*4+3]=255;}}
void RefreshImage(){RebuildDib(app.current,app.currentDib);app.statistics=Analyze(app.current);app.grayscale=app.current.gray();SetWindowTextW(app.info,app.details.c_str());EnableActions();InvalidateRect(app.window,nullptr,FALSE);}
std::wstring Number(double value){std::wostringstream s;s<<value;return s.str();}
void SetKernelPreset(int preset){
    const wchar_t* values[]={L"0  0  0\r\n0  1  0\r\n0  0  0",L"1  1  1\r\n1  1  1\r\n1  1  1",L"1  2  1\r\n2  4  2\r\n1  2  1",L"0  -1  0\r\n-1  5  -1\r\n0  -1  0",L"-1  0  1\r\n-2  0  2\r\n-1  0  1",L"0  -1  0\r\n-1  4  -1\r\n0  -1  0",L"-2  -1  0\r\n-1  1  1\r\n0  1  2"};
    SendMessageW(app.kernelSize,CB_SETCURSEL,1,0);SetWindowTextW(app.kernelEdit,values[std::clamp(preset,0,6)]);SetWindowTextW(app.bias,preset==6?L"128":L"0");SetWindowTextW(app.divisor,L"1");
}
void Layout(){
    RECT client{};GetClientRect(app.window,&client);app.width=MulDiv(client.right,96,app.dpi);app.height=MulDiv(client.bottom,96,app.dpi);
    int w=app.width,h=app.height;
    Place(GetDlgItem(app.window,Open),w-615,20,98,32);Place(GetDlgItem(app.window,Demo),w-509,20,76,32);
    Place(GetDlgItem(app.window,Undo),w-425,20,68,32);Place(GetDlgItem(app.window,Reset),w-349,20,88,32);Place(GetDlgItem(app.window,Save),w-253,20,132,32);Place(GetDlgItem(app.window,Help),w-113,20,89,32);
    Place(app.category,22,109,272,300);
    const auto& desc=Describe(app.selected);bool custom=app.selected==Op::Custom;
    int listHeight=std::clamp(118+(h-720)/5,108,162),listY=173,descY=custom?153:listY+listHeight+10;
    ShowWindow(app.list,custom?SW_HIDE:SW_SHOW);
    Place(app.list,22,listY,272,listHeight);Place(app.explanation,22,descY,272,49);
    int selected=int(SendMessageW(app.list,LB_GETCURSEL,0,0));RECT selectedRect{},listClient{};GetClientRect(app.list,&listClient);
    if(selected>=0&&SendMessageW(app.list,LB_GETITEMRECT,selected,reinterpret_cast<LPARAM>(&selectedRect))!=LB_ERR&&(selectedRect.top<0||selectedRect.bottom>listClient.bottom))SendMessageW(app.list,LB_SETTOPINDEX,selected,0);
    int paramsY=descY+59;
    for(int i=0;i<4;++i){bool visible=!custom&&size_t(i)<desc.parameters.size();ShowWindow(app.labels[i],visible?SW_SHOW:SW_HIDE);ShowWindow(app.edits[i],visible?SW_SHOW:SW_HIDE);int x=22+(i%2)*141,y=paramsY+(i/2)*63;Place(app.labels[i],x,y,132,21);Place(app.edits[i],x,y+24,131,29);}
    for(auto control:{app.preset,app.kernelSize,app.kernelEdit,app.normalize,app.flip,app.color,app.divisor,app.bias,app.border,app.divLabel,app.biasLabel,app.borderLabel})ShowWindow(control,custom?SW_SHOW:SW_HIDE);
    int matrixHeight=std::clamp(86+(h-720)/3,78,145);
    Place(app.preset,22,paramsY,182,240);Place(app.kernelSize,212,paramsY,82,240);
    Place(app.kernelEdit,22,paramsY+36,272,matrixHeight);
    int y=paramsY+36+matrixHeight+8;
    Place(app.normalize,22,y,160,23);Place(app.color,187,y,107,23);Place(app.flip,22,y+26,272,23);
    Place(app.divLabel,22,y+56,42,24);Place(app.divisor,65,y+53,76,28);Place(app.biasLabel,164,y+56,42,24);Place(app.bias,207,y+53,87,28);
    Place(app.borderLabel,22,y+90,57,24);Place(app.border,84,y+86,210,240);
    Place(GetDlgItem(app.window,Apply),22,h-83,272,38);
    Place(app.zoom,w-132,84,108,240);
    Place(app.info,644,h-153,std::max(100,w-668),110);
    InvalidateRect(app.window,nullptr,TRUE);
}
void SelectOperation(){
    int index=int(SendMessageW(app.list,LB_GETCURSEL,0,0));if(index<0||index>=int(app.visible.size()))return;
    app.selected=app.visible[index];const auto& desc=Describe(app.selected);SetWindowTextW(app.explanation,desc.explanation.c_str());
    for(size_t i=0;i<desc.parameters.size();++i){SetWindowTextW(app.labels[i],desc.parameters[i].name.c_str());SetWindowTextW(app.edits[i],Number(desc.parameters[i].initial).c_str());}
    Layout();
}
void SelectCategory(){
    int category=int(SendMessageW(app.category,CB_GETCURSEL,0,0));app.visible.clear();SendMessageW(app.list,LB_RESETCONTENT,0,0);
    for(const auto& op:Operations())if(op.category==category){app.visible.push_back(op.id);SendMessageW(app.list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(op.name.c_str()));}
    SendMessageW(app.list,LB_SETCURSEL,0,0);SelectOperation();
}
double Numeric(HWND edit,const wchar_t* name){auto text=Text(edit);size_t used=0;double v;
    try{v=std::stod(text,&used);}catch(...){throw std::runtime_error(Utf8Message(std::wstring(name)+L"不是有效数字，请重新输入。"));}
    if(text.find_first_not_of(L" \t\r\n",used)!=std::wstring::npos||!std::isfinite(v))throw std::runtime_error(Utf8Message(std::wstring(name)+L"含无效字符。"));return v;
}
void Error(const std::wstring& message){app.status=message;MessageBoxW(app.window,message.c_str(),L"无法完成操作",MB_OK|MB_ICONWARNING);InvalidateRect(app.window,nullptr,FALSE);}
void StartJob(int kind,const std::wstring& path={}){
    if(app.busy)return;
    Op op=app.selected;std::vector<double> params;Kernel kernel;
    try{
        if(kind==1){for(size_t i=0;i<Describe(op).parameters.size();++i){const auto& spec=Describe(op).parameters[i];double value=Numeric(app.edits[i],spec.name.c_str());if(value<spec.minimum||value>spec.maximum||(spec.integer&&std::floor(value)!=value))throw std::runtime_error(Utf8Message(spec.name+L"须为 "+Number(spec.minimum)+L" 到 "+Number(spec.maximum)+(spec.integer?L" 之间的整数。":L" 之间的数值。")));params.push_back(value);}
            if(op==Op::Custom){int index=int(SendMessageW(app.kernelSize,CB_GETCURSEL,0,0));kernel=ParseKernel(Text(app.kernelEdit),1+2*std::max(0,index));kernel.normalize=SendMessageW(app.normalize,BM_GETCHECK,0,0)==BST_CHECKED;kernel.flip=SendMessageW(app.flip,BM_GETCHECK,0,0)==BST_CHECKED;kernel.color=SendMessageW(app.color,BM_GETCHECK,0,0)==BST_CHECKED;kernel.divisor=Numeric(app.divisor,L"除数");kernel.bias=Numeric(app.bias,L"偏移");kernel.border=int(SendMessageW(app.border,CB_GETCURSEL,0,0));}
        }
    }catch(const std::exception& e){Error(ErrorText(e));return;}
    app.busy=true;app.cancelled=false;app.status=kind==0?L"正在读取图像…":kind==2?L"正在保存 BMP…":L"正在处理："+Describe(op).name+L"…";EnableActions();
    Image input;if(kind!=0)input=app.current;HWND window=app.window;
    app.worker=std::thread([kind,path,op,params=std::move(params),kernel=std::move(kernel),input=std::move(input),window]()mutable{
        auto job=std::make_unique<JobResult>();job->kind=kind;job->path=path;job->name=Describe(op).name;auto begin=std::chrono::steady_clock::now();
        try{if(kind==0)job->result.image=LoadImage(path,&job->route);else if(kind==2)SaveBmp(input,path);else job->result=Process(input,op,params,kernel,&app.cancelled);}catch(const std::exception& e){job->result.error=ErrorText(e);}
        job->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        if(PostMessageW(window,Finished,0,reinterpret_cast<LPARAM>(job.get())))job.release();
    });
}
void LoadDemo(){if(app.busy)return;app.original=DemoImage();app.current=app.original;app.path=L"演示图（程序生成）";app.operation=L"原图";app.details=L"演示图包含渐变、色块、细线与封闭形状，可用于尝试边缘检测、阈值分割和自定义卷积。";app.history.clear();app.dirty=false;app.zoomFactor=0;app.pan={};RebuildDib(app.original,app.originalDib);app.status=L"已载入演示图。选择左侧算法，然后应用。";RefreshImage();}
bool PickFile(bool save,std::wstring& path){
    ComPtr<IFileDialog> dialog;HRESULT hr;
    if(save){ComPtr<IFileSaveDialog> d;hr=CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d));if(SUCCEEDED(hr))d.As(&dialog);}
    else{ComPtr<IFileOpenDialog> d;hr=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d));if(SUCCEEDED(hr))d.As(&dialog);}
    if(FAILED(hr)){Error(L"无法打开系统文件选择窗口。");return false;}
    COMDLG_FILTERSPEC openTypes[]={{L"常见图像",L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.gif;*.ico;*.jxr;*.wdp;*.webp;*.heic;*.heif"},{L"所有文件",L"*.*"}},saveTypes[]={{L"BMP 位图",L"*.bmp"}};
    dialog->SetFileTypes(save?1:2,save?saveTypes:openTypes);dialog->SetDefaultExtension(save?L"bmp":L"png");
    DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|(save?FOS_OVERWRITEPROMPT:FOS_FILEMUSTEXIST));
    dialog->SetTitle(save?L"将当前结果另存为 BMP":L"打开图像");
    if(save){std::wstring name=app.path.empty()?L"处理结果":std::filesystem::path(app.path).stem().wstring();name+=L"_处理结果.bmp";dialog->SetFileName(name.c_str());}
    if(dialog->Show(app.window)!=S_OK)return false;ComPtr<IShellItem> item;if(FAILED(dialog->GetResult(&item)))return false;
    PWSTR name=nullptr;if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&name)))return false;path=name;CoTaskMemFree(name);
    if(save&&std::filesystem::path(path).extension()!=L".bmp"&&std::filesystem::path(path).extension()!=L".BMP"){
        std::filesystem::path corrected(path);corrected.replace_extension(L".bmp");
        if(std::filesystem::exists(corrected)&&MessageBoxW(app.window,L"BMP 文件名已自动修正。该文件已存在，是否覆盖？",L"另存 BMP",MB_YESNO|MB_ICONQUESTION)!=IDYES)return false;path=corrected.wstring();
    }
    return true;
}
void RunPendingAction(){auto action=app.pending;auto path=std::move(app.pendingPath);app.pending=PendingAction::None;app.pendingPath.clear();
    if(action==PendingAction::OpenDialog){std::wstring filename;if(PickFile(false,filename))StartJob(0,filename);}
    else if(action==PendingAction::OpenPath)StartJob(0,path);
    else if(action==PendingAction::Demo)LoadDemo();
    else if(action==PendingAction::Close)DestroyWindow(app.window);
}
void ResolveUnsavedChoice(int choice,const std::wstring* chosenSavePath=nullptr){
    if(choice==IDNO){RunPendingAction();return;}
    if(choice==IDYES){std::wstring filename;bool chosen=chosenSavePath?!(filename=*chosenSavePath).empty():PickFile(true,filename);if(chosen){StartJob(2,filename);return;}}
    app.pending=PendingAction::None;app.pendingPath.clear();
}
void RequestAction(PendingAction action,const std::wstring& path={}){
    if(app.busy)return;app.pending=action;app.pendingPath=path;
    if(!app.dirty||!app.current.valid()){RunPendingAction();return;}
    int choice=MessageBoxW(app.window,L"当前处理结果尚未保存。\n\n是否先保存为 BMP？\n选择“是”保存后继续；选择“否”放弃当前结果；选择“取消”留在当前图像。",L"保存当前处理结果",MB_YESNOCANCEL|MB_ICONQUESTION|MB_DEFBUTTON3);
    ResolveUnsavedChoice(choice);
}
bool UndoFocusedEdit(){HWND focus=GetFocus();wchar_t type[32]{};GetClassNameW(focus,type,32);
    if(_wcsicmp(type,L"Edit")==0&&!(GetWindowLongPtrW(focus,GWL_STYLE)&ES_READONLY)){SendMessageW(focus,EM_UNDO,0,0);return true;}return false;
}
void PushHistory(){
    app.history.push_back({std::move(app.current),app.operation,app.details});size_t bytes=0;for(const auto& s:app.history)bytes+=s.image.bgr.size();
    while(app.history.size()>1&&(bytes>128ULL*1024*1024||app.history.size()>10)){bytes-=app.history.front().image.bgr.size();app.history.erase(app.history.begin());}
}
void UndoImage(){if(app.busy||app.history.empty())return;auto previous=std::move(app.history.back());app.history.pop_back();app.current=std::move(previous.image);app.operation=previous.name;app.details=previous.details;app.dirty=true;app.status=L"已撤销上一次处理。";RefreshImage();}
void ResetImage(){if(app.busy||!app.original.valid())return;PushHistory();app.current=app.original;app.operation=L"原图";app.details=L"已恢复导入时的原始图像。";app.dirty=false;app.status=L"已恢复原图；仍可撤销此次恢复。";RefreshImage();}
void FinishedJob(JobResult& job){
    if(app.worker.joinable())app.worker.join();app.busy=false;
    if(!job.result.error.empty()){app.lastError=job.result.error;app.pending=PendingAction::None;app.pendingPath.clear();EnableActions();if(app.test){if(!app.expectedTestError)++app.testFailures;app.expectedTestError=false;app.status=job.result.error;}else Error(job.result.error);return;}
    if(job.kind==0){app.original=std::move(job.result.image);app.current=app.original;app.path=job.path;app.history.clear();app.operation=L"原图";app.details=L"导入："+app.path+L"\r\n"+job.route;app.zoomFactor=0;app.pan={};app.dirty=false;RebuildDib(app.original,app.originalDib);app.status=L"图像读取完成。";}
    else if(job.kind==2){app.dirty=false;app.status=L"BMP 已保存："+job.path;EnableActions();RunPendingAction();return;}
    else{PushHistory();app.current=std::move(job.result.image);app.operation=job.name;app.details=job.result.details;app.dirty=true;std::wostringstream s;s<<L"已完成 "<<job.name<<L" · "<<std::fixed<<std::setprecision(0)<<job.milliseconds<<L" ms";app.status=s.str();}
    RefreshImage();
}
void Fill(HDC dc,RECT r,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
void Label(HDC dc,const std::wstring& text,RECT r,COLORREF color,HFONT font,UINT format=DT_LEFT|DT_VCENTER|DT_SINGLELINE){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);auto old=SelectObject(dc,font);DrawTextW(dc,text.c_str(),int(text.size()),&r,format);SelectObject(dc,old);}
RECT R(int x,int y,int right,int bottom){return {app.S(x),app.S(y),app.S(right),app.S(bottom)};}
void DrawImage(HDC dc,const Image& image,const std::vector<BYTE>& dib,RECT rect){
    if(!image.valid())return;int rw=rect.right-rect.left,rh=rect.bottom-rect.top;double fit=std::min(double(rw)/image.width,double(rh)/image.height),scale=app.zoomFactor>0?app.zoomFactor:fit;
    int width=std::max(1,int(image.width*scale)),height=std::max(1,int(image.height*scale));int x=rect.left+(rw-width)/2+app.pan.x,y=rect.top+(rh-height)/2+app.pan.y;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image.width;info.bmiHeader.biHeight=-image.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    int saved=SaveDC(dc);IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);SetStretchBltMode(dc,scale<1?HALFTONE:COLORONCOLOR);SetBrushOrgEx(dc,0,0,nullptr);StretchDIBits(dc,x,y,width,height,0,0,image.width,image.height,dib.data(),&info,DIB_RGB_COLORS,SRCCOPY);RestoreDC(dc,saved);
}
void Paint(HDC target){
    RECT client;GetClientRect(app.window,&client);HDC dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);auto old=SelectObject(dc,bitmap);
    int w=app.width,h=app.height;Fill(dc,client,Paper);Fill(dc,R(316,72,w,h-182),Stage);Fill(dc,R(0,71,w,72),Line);Fill(dc,R(315,72,316,h-30),Line);Fill(dc,R(0,h-31,w,h-30),Line);
    Fill(dc,R(24,25,32,33),Accent);Fill(dc,R(35,25,43,33),Accent);Fill(dc,R(24,36,32,44),Accent);Fill(dc,R(35,36,43,44),RGB(120,169,151));
    Label(dc,L"ile chan的图像工坊",R(55,14,360,43),Ink,app.title);Label(dc,L"图像处理与图像分析基础",R(55,44,340,62),Muted,app.smallFont);
    Label(dc,L"处理类别",R(22,84,294,105),Muted,app.smallFont);if(app.selected!=Op::Custom)Label(dc,L"选择算法",R(22,147,294,170),Muted,app.smallFont);
    if(app.selected!=Op::Custom){int descY=173+std::clamp(118+(h-720)/5,108,162)+10;int y=descY+59+(Describe(app.selected).parameters.empty()?10:((int(Describe(app.selected).parameters.size())+1)/2)*63+10);
        Label(dc,L"操作会接续当前结果。\n灰度算法会自动转换彩色图像。\n原图始终保留在预览区左栏。",R(22,y,294,y+66),Muted,app.smallFont,DT_LEFT|DT_WORDBREAK);}
    Label(dc,L"快捷键  Ctrl+O 打开  ·  Ctrl+S 保存",R(22,h-119,294,h-96),Muted,app.smallFont);
    if(app.current.valid()){
        int available=w-316,half=available/2;
        Label(dc,L"原图",R(340,83,500,106),RGB(236,242,238),app.body);Label(dc,L"当前结果 · "+app.operation,R(340+half,83,w-150,106),RGB(236,242,238),app.body,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        Fill(dc,R(316+half,116,317+half,h-198),RGB(62,69,69));
        DrawImage(dc,app.original,app.originalDib,R(340,122,316+half-24,h-201));DrawImage(dc,app.current,app.currentDib,R(340+half,122,w-24,h-201));
        Label(dc,std::to_wstring(app.original.width)+L" × "+std::to_wstring(app.original.height),R(340,h-207,600,h-184),RGB(164,182,174),app.smallFont);
        Label(dc,std::to_wstring(app.current.width)+L" × "+std::to_wstring(app.current.height)+(app.grayscale?L"  灰度":L"  彩色"),R(340+half,h-207,w-24,h-184),RGB(164,182,174),app.smallFont);
        Label(dc,L"当前灰度分布",R(340,h-171,600,h-149),Muted,app.smallFont);
        int peak=*std::max_element(app.statistics.histogram.begin(),app.statistics.histogram.end());
        for(int i=0;i<256;++i){int bar=peak?int(65.0*app.statistics.histogram[i]/peak):0;Fill(dc,R(340+i,h-71-bar,341+i,h-71),Accent);}
        Fill(dc,R(340,h-71,596,h-70),Line);
        std::wostringstream stats;stats<<std::fixed<<std::setprecision(1)<<L"均值 "<<app.statistics.mean<<L"  标准差 "<<app.statistics.contrast<<L"  范围 "<<app.statistics.low<<L"–"<<app.statistics.high;
        Label(dc,stats.str(),R(340,h-66,630,h-42),Muted,app.smallFont);
        Label(dc,L"处理记录与测量结果",R(644,h-179,w-24,h-155),Muted,app.smallFont);
    }else{
        int middle=316+(w-316)/2;
        Label(dc,L"先打开一张图像",R(middle-220,260,middle+220,300),RGB(238,244,240),app.title,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        Label(dc,L"选择文件后，在左侧选择算法并调整参数。",R(middle-300,307,middle+300,337),RGB(176,195,185),app.body,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        Label(dc,L"PNG  ·  JPEG  ·  BMP  ·  TIFF  ·  GIF  ·  ICO",R(middle-300,344,middle+300,371),RGB(176,195,185),app.smallFont,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        Label(dc,L"也可将图像文件拖到窗口中，或使用顶部演示图。",R(middle-300,380,middle+300,407),RGB(176,195,185),app.smallFont,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    Label(dc,app.status,R(22,h-28,w-24,h-3),app.busy?Accent:Muted,app.smallFont,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
void Capture(const std::wstring& path){
    RECT rect;GetWindowRect(app.window,&rect);int w=rect.right-rect.left,h=rect.bottom-rect.top;HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);auto old=SelectObject(dc,bitmap);
    PrintWindow(app.window,dc,PW_RENDERFULLCONTENT);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;std::vector<BYTE> pixels(size_t(w)*h*4);SelectObject(dc,old);GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    ComPtr<IWICImagingFactory> factory;CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));ComPtr<IWICStream> stream;factory->CreateStream(&stream);if(FAILED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE)))return;
    ComPtr<IWICBitmapEncoder> encoder;factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);ComPtr<IWICBitmapFrameEncode> frame;encoder->CreateNewFrame(&frame,nullptr);frame->Initialize(nullptr);frame->SetSize(w,h);WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;frame->SetPixelFormat(&format);
    ComPtr<IWICBitmap> source;factory->CreateBitmapFromMemory(w,h,GUID_WICPixelFormat32bppBGR,w*4,UINT(pixels.size()),pixels.data(),&source);frame->WriteSource(source.Get(),nullptr);frame->Commit();encoder->Commit();
}
void UiTestTick(){
    if(app.busy)return;
    auto path=[&](const wchar_t* name){return (std::filesystem::path(app.testDirectory)/name).wstring();};
    if(app.testPhase==0){Capture(path(L"ui-empty.png"));LoadDemo();app.testPhase=1;}
    else if(app.testPhase==1){SendMessageW(app.category,CB_SETCURSEL,2,0);SelectCategory();SendMessageW(app.list,LB_SETCURSEL,2,0);SelectOperation();StartJob(1);app.testPhase=2;}
    else if(app.testPhase==2){Capture(path(L"ui-desktop.png"));UndoImage();app.testBefore=app.current.bgr;SendMessageW(app.category,CB_SETCURSEL,6,0);SelectCategory();SendMessageW(app.preset,CB_SETCURSEL,2,0);SetKernelPreset(2);app.testPhase=3;}
    else if(app.testPhase==3){Capture(path(L"ui-kernel.png"));StartJob(1);app.testPhase=4;}
    else if(app.testPhase==4){if(app.current.bgr==app.testBefore)++app.testFailures;UndoImage();if(app.current.bgr!=app.testBefore)++app.testFailures;SetWindowPos(app.window,nullptr,0,0,app.S(1080),app.S(760),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);app.testPhase=5;}
    else if(app.testPhase==5){Capture(path(L"ui-compact.png"));ResetImage();if(app.current.bgr!=app.original.bgr)++app.testFailures;
        std::wstring before=Text(app.kernelEdit);SetFocus(app.kernelEdit);SendMessageW(app.kernelEdit,EM_SETSEL,before.size(),before.size());SendMessageW(app.kernelEdit,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L"9"));size_t historySize=app.history.size();if(!UndoFocusedEdit()||Text(app.kernelEdit)!=before||app.history.size()!=historySize)++app.testFailures;
        app.dirty=true;app.pending=PendingAction::Demo;ResolveUnsavedChoice(IDCANCEL);if(app.pending!=PendingAction::None||!app.dirty||app.current.bgr!=app.original.bgr)++app.testFailures;
        app.pending=PendingAction::Close;ResolveUnsavedChoice(IDCANCEL);if(!IsWindow(app.window)||!app.dirty)++app.testFailures;
        std::wstring cancelPath;app.pending=PendingAction::Demo;ResolveUnsavedChoice(IDYES,&cancelPath);if(app.pending!=PendingAction::None||app.busy||!app.dirty)++app.testFailures;
        app.expectedTestError=true;app.lastError.clear();app.pending=PendingAction::Demo;std::wstring badPath=path(L"missing-directory\\failure.bmp");ResolveUnsavedChoice(IDYES,&badPath);app.testPhase=6;
    }
    else if(app.testPhase==6){if(app.lastError.empty()||!app.dirty||app.pending!=PendingAction::None||app.current.bgr!=app.original.bgr)++app.testFailures;app.pending=PendingAction::Demo;std::wstring savedPath=path(L"ui-saved.bmp");ResolveUnsavedChoice(IDYES,&savedPath);app.testPhase=7;}
    else if(app.testPhase==7){if(app.dirty||app.pending!=PendingAction::None||!std::filesystem::exists(path(L"ui-saved.bmp")))++app.testFailures;StartJob(0,path(L"ui-saved.bmp"));app.testPhase=8;}
    else if(app.testPhase==8){if(!app.current.valid()||app.current.bgr!=DemoImage().bgr)++app.testFailures;app.dirty=true;app.pending=PendingAction::Demo;ResolveUnsavedChoice(IDNO);if(app.dirty||!app.history.empty())++app.testFailures;
        std::wstring report=L"Native UI smoke test: "+std::wstring(app.testFailures?L"FAIL":L"PASS")+L"\r\nOpen demo, Sobel background apply, undo, custom Gaussian apply, undo, resize, reset.\r\nFocused text undo preserves image history.\r\nUnsaved: Cancel, cancelled Save dialog, failed Save, successful Save then replace, discard.\r\nNative background BMP save and reimport.\r\n";
        HANDLE file=CreateFileW(path(L"ui-test.txt").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD bytes=0;WriteFile(file,report.data(),DWORD(report.size()*sizeof(wchar_t)),&bytes,nullptr);CloseHandle(file);KillTimer(app.window,1);PostMessageW(app.window,WM_CLOSE,0,0);app.testPhase=9;}
}
void CreateControls(){
    FontSetup();
    for(auto pair:{std::pair<int,const wchar_t*>{Open,L"打开图像"},{Demo,L"演示图"},{Undo,L"撤销"},{Reset,L"恢复原图"},{Save,L"另存 BMP"},{Help,L"使用帮助"},{Apply,L"应用到当前图像   F5"}})Control(L"BUTTON",pair.second,BS_OWNERDRAW|WS_TABSTOP,pair.first);
    app.category=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Category);
    for(auto name:categories)SendMessageW(app.category,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(app.category,CB_SETCURSEL,0,0);
    app.list=Control(WC_LISTBOXW,L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_TABSTOP|WS_BORDER,Algorithms);
    SendMessageW(app.list,LB_SETITEMHEIGHT,0,app.S(26));
    app.explanation=Control(L"STATIC",L"",SS_LEFT,0);SendMessageW(app.explanation,WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);
    for(int i=0;i<4;++i){app.labels[i]=Control(L"STATIC",L"",SS_LEFT,0);SendMessageW(app.labels[i],WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);app.edits[i]=Control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,Param0+i);SendMessageW(app.edits[i],EM_SETLIMITTEXT,24,0);}
    app.zoom=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,Zoom);for(auto name:{L"适应窗口",L"25%",L"50%",L"100%",L"200%"})SendMessageW(app.zoom,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(app.zoom,CB_SETCURSEL,0,0);
    app.preset=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,Preset);for(auto name:{L"恒等核",L"均值模糊",L"高斯模糊",L"锐化",L"Sobel X",L"拉普拉斯",L"浮雕"})SendMessageW(app.preset,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(app.preset,CB_SETCURSEL,0,0);
    app.kernelSize=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,KernelSize);for(int i=1;i<=15;i+=2){auto name=std::to_wstring(i)+L" × "+std::to_wstring(i);SendMessageW(app.kernelSize,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}
    app.kernelEdit=Control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|ES_WANTRETURN,KernelEdit);SendMessageW(app.kernelEdit,EM_SETLIMITTEXT,16000,0);SendMessageW(app.kernelEdit,WM_SETFONT,reinterpret_cast<WPARAM>(app.matrix),TRUE);
    app.normalize=Control(L"BUTTON",L"按系数和归一化",BS_AUTOCHECKBOX|WS_TABSTOP,Normalize);app.flip=Control(L"BUTTON",L"翻转卷积核（数学卷积）",BS_AUTOCHECKBOX|WS_TABSTOP,Flip);app.color=Control(L"BUTTON",L"保留彩色",BS_AUTOCHECKBOX|WS_TABSTOP,Color);
    for(auto check:{app.normalize,app.flip,app.color})SendMessageW(check,BM_SETCHECK,BST_CHECKED,0);
    app.divLabel=Control(L"STATIC",L"除数",0,0);app.biasLabel=Control(L"STATIC",L"偏移",0,0);app.borderLabel=Control(L"STATIC",L"边界",0,0);
    app.divisor=Control(L"EDIT",L"1",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,Divisor);app.bias=Control(L"EDIT",L"0",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,Bias);
    app.border=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,BorderMode);for(auto name:{L"复制边缘",L"镜像边缘",L"补零"})SendMessageW(app.border,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(app.border,CB_SETCURSEL,0,0);
    app.info=Control(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,Info);SendMessageW(app.info,WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);
    SetKernelPreset(0);SelectCategory();EnableActions();
}
LRESULT CALLBACK WindowProc(HWND window,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:app.window=window;app.dpi=GetDpiForWindow(window);CreateControls();DragAcceptFiles(window,TRUE);return 0;
    case WM_SIZE:if(app.window)Layout();return 0;
    case WM_DPICHANGED:{app.dpi=HIWORD(wp);FontSetup();auto rect=reinterpret_cast<RECT*>(lp);SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
    case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);info->ptMinTrackSize={std::min<LONG>(app.S(1080),work.right-work.left-24),std::min<LONG>(app.S(680),work.bottom-work.top-24)};return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint;auto dc=BeginPaint(window,&paint);Paint(dc);EndPaint(window,&paint);return 0;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,Ink);SetBkColor(dc,Paper);return reinterpret_cast<LRESULT>(app.paper);}
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,Ink);SetBkColor(dc,RGB(255,255,255));return reinterpret_cast<LRESULT>(app.white);}
    case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlType!=ODT_BUTTON)break;bool primary=item->CtlID==Apply||item->CtlID==Save,disabled=item->itemState&ODS_DISABLED,pressed=item->itemState&ODS_SELECTED;
        COLORREF background=disabled?RGB(226,231,227):primary?(pressed?RGB(7,77,63):Accent):(pressed?RGB(217,229,221):RGB(232,238,233));Fill(item->hDC,item->rcItem,background);auto text=Text(item->hwndItem);Label(item->hDC,text,item->rcItem,disabled?RGB(120,133,124):primary?RGB(255,255,255):Ink,app.body,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(item->itemState&ODS_FOCUS){auto focus=item->rcItem;InflateRect(&focus,-3,-3);DrawFocusRect(item->hDC,&focus);}return TRUE;}
    case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);
        if(id==Category&&event==CBN_SELCHANGE){SelectCategory();return 0;}if(id==Algorithms&&event==LBN_SELCHANGE){SelectOperation();return 0;}
        if(id==Preset&&event==CBN_SELCHANGE){SetKernelPreset(int(SendMessageW(app.preset,CB_GETCURSEL,0,0)));return 0;}
        if(id==Normalize&&event==BN_CLICKED){EnableActions();return 0;}
        if(id==KernelSize&&event==CBN_SELCHANGE){int s=1+2*int(SendMessageW(app.kernelSize,CB_GETCURSEL,0,0));std::wstring matrix;for(int y=0;y<s;++y){for(int x=0;x<s;++x)matrix+=(x==s/2&&y==s/2?L"1  ":L"0  ");if(y+1<s)matrix+=L"\r\n";}SetWindowTextW(app.kernelEdit,matrix.c_str());return 0;}
        if(id==Zoom&&event==CBN_SELCHANGE){double values[]={0,.25,.5,1,2};int at=int(SendMessageW(app.zoom,CB_GETCURSEL,0,0));app.zoomFactor=values[std::clamp(at,0,4)];app.pan={};InvalidateRect(window,nullptr,FALSE);return 0;}
        if(event==BN_CLICKED||event==0){if(id==Open&&!app.busy)RequestAction(PendingAction::OpenDialog);else if(id==Save&&!app.busy&&app.current.valid()){std::wstring path;if(PickFile(true,path))StartJob(2,path);}else if(id==Apply&&!app.busy&&app.current.valid())StartJob(1);else if(id==Undo)UndoImage();else if(id==Reset)ResetImage();else if(id==Demo)RequestAction(PendingAction::Demo);
            else if(id==Help)MessageBoxW(window,L"1. 打开或拖入图像，选择左侧类别与算法。\n2. 调整参数，按 F5 应用；操作会接续当前结果。\n3. Ctrl+Z 撤销，恢复原图也可撤销。\n4. Ctrl+S 将当前结果另存为 BMP。\n\n自定义卷积：每行输入系数，支持空格、逗号；默认按系数和归一化，零和核使用除数 1。取消归一化可手动设置除数。\n\n缩放后可拖动图像；鼠标滚轮缩放。\n\n原生支持 PNG/JPEG/BMP/TIFF/GIF/ICO；GIF、TIFF 读取第一帧，透明区域以白色合成。WebP/HEIF 等格式取决于系统已安装的 WIC 编解码器。\n\n灰度算法自动转灰度；反色和自定义卷积可保留彩色。轮廓功能以白色为前景，必要时先反色。\n最多 2000 万像素；历史记录最多 10 步，并按 128 MB 预算自动保留。",L"ile chan的图像工坊 · 使用帮助",MB_OK|MB_ICONINFORMATION);
        }return 0;}
    case Finished:{std::unique_ptr<JobResult> job(reinterpret_cast<JobResult*>(lp));FinishedJob(*job);return 0;}
    case WM_DROPFILES:{HDROP drop=reinterpret_cast<HDROP>(wp);wchar_t path[32768]{};if(!app.busy&&DragQueryFileW(drop,0,path,32768))RequestAction(PendingAction::OpenPath,path);DragFinish(drop);return 0;}
    case WM_LBUTTONDOWN:if(GET_X_LPARAM(lp)>app.S(316)&&GET_Y_LPARAM(lp)>app.S(116)&&GET_Y_LPARAM(lp)<app.S(app.height-182)&&app.current.valid()){app.dragging=true;app.dragStart={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};app.panStart=app.pan;SetCapture(window);}return 0;
    case WM_MOUSEMOVE:if(app.dragging){app.pan.x=app.panStart.x+GET_X_LPARAM(lp)-app.dragStart.x;app.pan.y=app.panStart.y+GET_Y_LPARAM(lp)-app.dragStart.y;InvalidateRect(window,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:if(app.dragging){app.dragging=false;ReleaseCapture();}return 0;
    case WM_MOUSEWHEEL:if(app.current.valid()){
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&point);if(point.x<app.S(316))break;
        double fit=std::min(double(app.S((app.width-316)/2-48))/app.current.width,double(app.S(app.height-323))/app.current.height);app.zoomFactor=std::clamp((app.zoomFactor>0?app.zoomFactor:fit)*(GET_WHEEL_DELTA_WPARAM(wp)>0?1.2:1/1.2),0.01,16.0);SendMessageW(app.zoom,CB_SETCURSEL,WPARAM(-1),0);SetWindowTextW(app.zoom,(Number(std::round(app.zoomFactor*100))+L"%").c_str());InvalidateRect(window,nullptr,FALSE);return 0;}break;
    case WM_TIMER:if(app.test)UiTestTick();return 0;
    case WM_CLOSE:if(app.busy){app.cancelled=true;if(app.worker.joinable())app.worker.join();MSG pending;while(PeekMessageW(&pending,window,Finished,Finished,PM_REMOVE)){std::unique_ptr<JobResult> job(reinterpret_cast<JobResult*>(pending.lParam));if(job->result.error.empty())FinishedJob(*job);else app.busy=false;}}if(IsWindow(window)){EnableActions();RequestAction(PendingAction::Close);}return 0;
    case WM_DESTROY:PostQuitMessage(app.testFailures?1:0);return 0;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    app.dpi=GetDpiForSystem();
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>=3&&std::wstring(argv[1])==L"--self-test"){int code=RunSelfTests(argv[2]);LocalFree(argv);CoUninitialize();return code;}
    if(argc>=3&&std::wstring(argv[1])==L"--ui-test"){app.test=true;app.testDirectory=argv[2];std::filesystem::create_directories(app.testDirectory);}
    std::wstring startup;if(argc>=2&&!app.test&&argv[1][0]!=L'-')startup=argv[1];LocalFree(argv);
    INITCOMMONCONTROLSEX controls{sizeof controls,ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    WNDCLASSEXW cls{sizeof cls};cls.lpfnWndProc=WindowProc;cls.hInstance=instance;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);cls.lpszClassName=L"IleChanImageStudio.Native";RegisterClassExW(&cls);
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int width=std::min<int>(app.S(1440),work.right-work.left-40),height=std::min<int>(app.S(920),work.bottom-work.top-40);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"ile chan的图像工坊",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,(work.left+work.right-width)/2,(work.top+work.bottom-height)/2,width,height,nullptr,nullptr,instance,nullptr);
    if(!window){CoUninitialize();return 2;}ShowWindow(window,app.test?SW_SHOWNOACTIVATE:show);UpdateWindow(window);if(app.test)SetTimer(window,1,450,nullptr);else if(!startup.empty())StartJob(0,startup);
    ACCEL shortcuts[]={{FVIRTKEY|FCONTROL,'O',Open},{FVIRTKEY|FCONTROL,'S',Save},{FVIRTKEY|FCONTROL,'Z',Undo},{FVIRTKEY|FCONTROL,'R',Reset},{FVIRTKEY,VK_F5,Apply}};HACCEL accelerators=CreateAcceleratorTableW(shortcuts,int(std::size(shortcuts)));
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_KEYDOWN&&message.wParam=='Z'&&(GetKeyState(VK_CONTROL)&0x8000)&&UndoFocusedEdit())continue;if(!TranslateAcceleratorW(window,accelerators,&message)&&!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    DestroyAcceleratorTable(accelerators);for(auto font:{app.body,app.smallFont,app.title,app.matrix})DeleteObject(font);DeleteObject(app.paper);DeleteObject(app.white);CoUninitialize();return int(message.wParam);
}
