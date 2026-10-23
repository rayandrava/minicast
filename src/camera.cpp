#include "camera.hpp"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <algorithm>

namespace mini {
struct Camera::State {
    mutable std::mutex mutex;
    std::shared_ptr<const CameraFrame> frame;
    std::wstring message=L"Camera is off";
    std::vector<std::wstring> names;
    std::atomic<bool> stop{true},active{false};
    std::atomic<bool> notice{false};
};
Camera::Camera():state(std::make_shared<State>()){}
Camera::~Camera(){stop();if(worker.joinable())worker.join();}
bool Camera::running()const{return state->active;}
std::wstring Camera::status()const{std::lock_guard lock(state->mutex);return state->message;}
std::vector<std::wstring> Camera::devices()const{std::lock_guard lock(state->mutex);return state->names;}
std::shared_ptr<const CameraFrame> Camera::latest_frame()const{
    state->notice=false;
    std::lock_guard lock(state->mutex);return state->frame;
}
CameraFrame Camera::frame(bool mirror)const{
    auto current=latest_frame();CameraFrame out;if(current)out=*current;
    if(mirror)for(UINT y=0;y<out.height;y++)for(UINT x=0;x<out.width/2;x++)for(int c=0;c<4;c++)std::swap(out.pixels[(y*out.width+x)*4+c],out.pixels[(y*out.width+out.width-x-1)*4+c]);return out;
}
void Camera::stop(){state->stop=true;std::lock_guard lock(state->mutex);state->frame={};}
// Asynchronous SourceReader callbacks avoid a blocking ReadSample during close/unplug.
class ReaderCallback final:public IMFSourceReaderCallback {
    std::atomic<ULONG> refs{1};
public:
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    std::mutex mutex;Com<IMFSample> sample;HRESULT error=S_OK;DWORD flags=0;
    ~ReaderCallback(){CloseHandle(event);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,void** ppv)override{if(!ppv)return E_POINTER;if(riid==IID_IUnknown||riid==IID_IMFSourceReaderCallback){*ppv=static_cast<IMFSourceReaderCallback*>(this);AddRef();return S_OK;}*ppv=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto r=--refs;if(!r)delete this;return r;}
    HRESULT STDMETHODCALLTYPE OnReadSample(HRESULT hr,DWORD,DWORD f,LONGLONG,IMFSample* s)override{{std::lock_guard lock(mutex);error=hr;flags=f;sample.reset();if(s){s->AddRef();sample.p=s;}}SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnFlush(DWORD)override{SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnEvent(DWORD,IMFMediaEvent*)override{return S_OK;}
};
bool Camera::start(HWND receiver,UINT notification,int device){
    if(state->active)return false;if(worker.joinable())worker.join();state->stop=false;state->active=true;state->notice=false;
    {std::lock_guard lock(state->mutex);state->message=L"Opening camera…";state->frame={};}
    auto shared=state;
    worker=std::thread([shared,receiver,notification,device]()mutable{
        HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED),mf=MFStartup(MF_VERSION,MFSTARTUP_FULL);HRESULT hr=mf;
        auto message=[&](const std::wstring& text){std::lock_guard lock(shared->mutex);shared->message=text;PostMessageW(receiver,notification,0,0);};
        {
            Com<IMFAttributes> attrs;Com<IMFMediaSource> source;Com<IMFSourceReader> reader;Com<ReaderCallback> callback;callback.p=new ReaderCallback;
            IMFActivate** devices=nullptr;UINT32 count=0;
            if(SUCCEEDED(hr))hr=MFCreateAttributes(attrs.put(),2);
            if(SUCCEEDED(hr))hr=attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
            if(SUCCEEDED(hr))hr=MFEnumDeviceSources(attrs.p,&devices,&count);
            if(SUCCEEDED(hr)&&count==0){message(L"No webcam found. Connect a camera and try again.");hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);}
            if(SUCCEEDED(hr)){
                std::vector<std::wstring> names;for(UINT i=0;i<count;i++){WCHAR* name=nullptr;UINT32 len=0;if(SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,&name,&len))){names.emplace_back(name);CoTaskMemFree(name);}else names.push_back(L"Camera "+std::to_wstring(i+1));}
                {std::lock_guard lock(shared->mutex);shared->names=names;}
                device=std::clamp(device,0,int(count)-1);hr=devices[device]->ActivateObject(IID_IMFMediaSource,reinterpret_cast<void**>(source.put()));
            }
            if(devices){for(UINT i=0;i<count;i++)devices[i]->Release();CoTaskMemFree(devices);}
            if(SUCCEEDED(hr)){
                attrs.reset();hr=MFCreateAttributes(attrs.put(),3);
                if(SUCCEEDED(hr)){attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK,callback.p);attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE);hr=MFCreateSourceReaderFromMediaSource(source.p,attrs.p,reader.put());}
            }
            UINT width=0,height=0;LONG stride=0;
            if(SUCCEEDED(hr)){
                reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE);reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE);
                // Prefer a modest native mode before requesting RGB conversion.
                Com<IMFMediaType> chosen;UINT64 best=UINT64_MAX;
                for(DWORD i=0;i<128;i++){Com<IMFMediaType> t;if(FAILED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,i,t.put())))break;UINT w=0,h=0;MFGetAttributeSize(t.p,MF_MT_FRAME_SIZE,&w,&h);if(w&&h&&w<=1280&&h<=720){UINT64 d=1280ULL*720-UINT64(w)*h;if(d<best){best=d;chosen=std::move(t);}}}
                if(chosen)reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,chosen.p);
                Com<IMFMediaType> output;hr=MFCreateMediaType(output.put());if(SUCCEEDED(hr)){output->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);output->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32);hr=reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,output.p);}
                if(SUCCEEDED(hr)){Com<IMFMediaType> type;hr=reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,type.put());if(SUCCEEDED(hr)){MFGetAttributeSize(type.p,MF_MT_FRAME_SIZE,&width,&height);UINT32 raw=0;if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&raw)))stride=static_cast<LONG>(raw);else stride=width*4;if(!width||!height||width>1920||height>1080)hr=E_INVALIDARG;}}
            }
            if(FAILED(hr)&&hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))message(L"Camera unavailable. Check Windows camera privacy settings or other apps. "+error_text(hr));
            bool streamRequested=false;
            if(SUCCEEDED(hr)&&!shared->stop){streamRequested=true;hr=reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,nullptr,nullptr,nullptr);if(SUCCEEDED(hr))message(L"Live · "+std::to_wstring(width)+L" × "+std::to_wstring(height));}
            uint64_t seq=0;
            while(SUCCEEDED(hr)&&!shared->stop){
                if(WaitForSingleObject(callback->event,100)!=WAIT_OBJECT_0)continue;
                Com<IMFSample> sample;DWORD flags=0;{std::lock_guard lock(callback->mutex);sample=std::move(callback->sample);hr=callback->error;flags=callback->flags;}
                if(FAILED(hr)||flags&MF_SOURCE_READERF_ENDOFSTREAM){message(L"Camera disconnected or stopped.");break;}
                if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED){message(L"Camera format changed. Close and reopen the camera.");break;}
                if(sample){Com<IMFMediaBuffer> buffer;if(SUCCEEDED(sample->ConvertToContiguousBuffer(buffer.put()))){BYTE* bytes=nullptr;DWORD length=0;if(SUCCEEDED(buffer->Lock(&bytes,nullptr,&length))){
                    size_t pitch=static_cast<size_t>(std::abs(stride));if(pitch>=width*4&&length>=pitch*height){auto frame=std::make_shared<CameraFrame>();frame->width=width;frame->height=height;frame->sequence=++seq;frame->pixels.resize(size_t(width)*height*4);for(UINT y=0;y<height;y++){UINT sy=stride<0?height-y-1:y;memcpy(frame->pixels.data()+size_t(y)*width*4,bytes+size_t(sy)*pitch,width*4);}if(!shared->stop){std::lock_guard lock(shared->mutex);shared->frame=std::move(frame);}}
                    buffer->Unlock();if(!shared->notice.exchange(true))PostMessageW(receiver,notification,0,0);
                }}}
                if(!shared->stop)hr=reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,nullptr,nullptr,nullptr);
            }
            if(streamRequested&&FAILED(hr)&&!shared->stop)message(L"Camera stream stopped. Close and reopen the camera. "+error_text(hr));
            if(reader)reader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);if(source)source->Shutdown();
        }
        if(SUCCEEDED(mf))MFShutdown();if(SUCCEEDED(init))CoUninitialize();{std::lock_guard lock(shared->mutex);shared->frame={};}shared->active=false;PostMessageW(receiver,notification,0,0);
    });return true;
}
bool save_png(const std::filesystem::path& path,const CameraFrame& frame){
    const uint64_t stride=uint64_t(frame.width)*4,bytes=stride*frame.height;
    if(!frame.width||!frame.height||stride>UINT_MAX||bytes>UINT_MAX||bytes!=frame.pixels.size())return false;
    Com<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(factory.put()))))return false;
    Com<IWICBitmap> source;Com<IWICFormatConverter> converted;
    Com<IWICStream> stream;Com<IWICBitmapEncoder> encoder;Com<IWICBitmapFrameEncode> output;Com<IPropertyBag2> props;
    // Media Foundation RGB32 has no alpha. PNG normally negotiates 24bppBGR;
    // convert to the actual encoder format instead of rejecting that format or
    // treating the capture buffer's unused byte as transparency.
    HRESULT hr=factory->CreateBitmapFromMemory(frame.width,frame.height,GUID_WICPixelFormat32bppBGR,
        static_cast<UINT>(stride),static_cast<UINT>(bytes),const_cast<BYTE*>(frame.pixels.data()),source.put());
    if(SUCCEEDED(hr))hr=factory->CreateStream(stream.put());
    if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put());
    if(SUCCEEDED(hr))hr=encoder->Initialize(stream.p,WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(output.put(),props.put());
    if(SUCCEEDED(hr))hr=output->Initialize(props.p);
    if(SUCCEEDED(hr))hr=output->SetSize(frame.width,frame.height);
    WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;
    if(SUCCEEDED(hr))hr=output->SetPixelFormat(&format);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(converted.put());
    if(SUCCEEDED(hr))hr=converted->Initialize(source.p,format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr))hr=output->WriteSource(converted.p,nullptr);
    if(SUCCEEDED(hr))hr=output->Commit();
    if(SUCCEEDED(hr))hr=encoder->Commit();
    return SUCCEEDED(hr);
}
bool copy_frame(HWND owner,const CameraFrame& frame){
    if(frame.pixels.empty()||!OpenClipboard(owner))return false;size_t bytes=sizeof(BITMAPINFOHEADER)+frame.pixels.size();HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!mem){CloseClipboard();return false;}auto* ptr=static_cast<unsigned char*>(GlobalLock(mem));if(!ptr){GlobalFree(mem);CloseClipboard();return false;}BITMAPINFOHEADER header{};header.biSize=sizeof(header);header.biWidth=frame.width;header.biHeight=-static_cast<LONG>(frame.height);header.biPlanes=1;header.biBitCount=32;header.biCompression=BI_RGB;header.biSizeImage=static_cast<DWORD>(frame.pixels.size());memcpy(ptr,&header,sizeof(header));memcpy(ptr+sizeof(header),frame.pixels.data(),frame.pixels.size());GlobalUnlock(mem);bool ok=EmptyClipboard()&&SetClipboardData(CF_DIB,mem);if(!ok)GlobalFree(mem);CloseClipboard();return ok;
}
} // namespace mini
