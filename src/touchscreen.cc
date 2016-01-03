#include <linux/input.h>
#include <unistd.h>

#include <v8.h>
#include <node.h>
#include <nan.h>

using namespace v8;
using namespace node;

NAN_METHOD(Async);
void AsyncWork(uv_work_t* req);
void AsyncAfter(uv_work_t* req);

struct TouchInfo {
    int fileDescriptor;
    Nan::Callback *callback;

    bool error;
    std::string errorMessage;

    struct input_event inputEvents[64];
    int read;

    int x;
    int y;
    int pressure;

    bool stop;
};

NAN_METHOD(Async) {
    // Nan::HandleScope scope;

    if (info[0]->IsUndefined()) {
        return Nan::ThrowError("No parameters specified");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowError("First parameter should be a string");
    }

    if (info[1]->IsUndefined()) {
        return Nan::ThrowError("No callback function specified");
    }

    if (!info[1]->IsFunction()) {
        return Nan::ThrowError("Second argument should be a function");
    }

    Nan::Utf8String *path = new Nan::Utf8String(info[0]);

    TouchInfo* touchInfo = new TouchInfo();
    touchInfo->fileDescriptor = open(**path, O_RDONLY);
    touchInfo->callback = new Nan::Callback(info[1].As<Function>());
    touchInfo->error = false;
    touchInfo->stop = false;

    uv_work_t *req = new uv_work_t();
    req->data = touchInfo;

    int status = uv_queue_work(uv_default_loop(), req, AsyncWork, (uv_after_work_cb)AsyncAfter);

    assert(status == 0);

    info.GetReturnValue().SetUndefined();
}

void AsyncWork(uv_work_t* req) {
    TouchInfo* touchInfo = static_cast<TouchInfo*>(req->data);

    touchInfo->read = read(touchInfo->fileDescriptor, touchInfo->inputEvents, sizeof(struct input_event) * 64);

    if (touchInfo->read < (int) sizeof(struct input_event)) {
        touchInfo->error = true;
        touchInfo->errorMessage = "Read problem";
    }
}

void AsyncAfter(uv_work_t* req) {
    Nan::HandleScope scope;

    TouchInfo* touchInfo = static_cast<TouchInfo*>(req->data);

    if (touchInfo->error) {
        Local<Value> err = Exception::Error(Nan::New<String>(touchInfo->errorMessage.c_str()).ToLocalChecked());

        const unsigned argc = 1;
        Local<Value> argv[argc] = { err };

        TryCatch try_catch;

        touchInfo->callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);

        if (try_catch.HasCaught()) {
            FatalException(try_catch);
        }
    } else {
        for (unsigned i = 0; i < touchInfo->read / sizeof(struct input_event); i++) {
            if (touchInfo->inputEvents[i].type == EV_SYN) {
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_Y)) {
                touchInfo->x = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_X)) {
                touchInfo->y = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_PRESSURE)) {
                touchInfo->pressure = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_KEY && (touchInfo->inputEvents[i].code == BTN_TOUCH)) {
                 Local<Object> touch = Nan::New<Object>();
                 touch->Set(Nan::New<String>("x").ToLocalChecked(), Nan::New<Number>(touchInfo->x));
                 touch->Set(Nan::New<String>("y").ToLocalChecked(), Nan::New<Number>(touchInfo->y));
                 touch->Set(Nan::New<String>("pressure").ToLocalChecked(), Nan::New<Number>(touchInfo->pressure));
                 touch->Set(Nan::New<String>("touch").ToLocalChecked(), Nan::New<Number>(touchInfo->inputEvents[i].value));
                 touch->Set(Nan::New<String>("stop").ToLocalChecked(), Nan::New<Boolean>(touchInfo->stop));

                 const unsigned argc = 2;
                 Local<Value> argv[argc] = { Nan::Null(), touch };

                 TryCatch try_catch;

                 touchInfo->callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
                 touchInfo->stop = touch->Get(Nan::New<String>("stop").ToLocalChecked())->BooleanValue();

                 if (try_catch.HasCaught()) {
                     FatalException(try_catch);
                 }
            }
        }

        if (touchInfo->stop) {
            delete touchInfo->callback;
            delete touchInfo;
            delete req;
        } else {
            int status = uv_queue_work(uv_default_loop(), req, AsyncWork, (uv_after_work_cb)AsyncAfter);
            assert(status == 0);
        }
    }
}

void InitAll(Handle<Object> exports, Handle<Object> module) {
    Nan::HandleScope scope;

    module->Set(Nan::New("exports").ToLocalChecked(),
      Nan::New<FunctionTemplate>(Async)->GetFunction());
}

NODE_MODULE(ts, InitAll)
