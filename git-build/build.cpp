#include <ecs/ecs.h>
#include <ecs/perf.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

#include <script.h>

#include <curl/curl.h>

#include <cstdlib>
#include <filesystem>

PULL_ESC_CORE;

namespace fs = std::filesystem;

struct WriteCallbackData
{
  char *memory = nullptr;
  size_t size = 0;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct WriteCallbackData *mem = (struct WriteCallbackData *)userp;
 
  char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr)
  {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  ::memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

struct ReadCallbackData
{
  char *readptr = nullptr;
  size_t sizeleft = 0;
};

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
  struct ReadCallbackData *wt = (struct ReadCallbackData *)userp;
  size_t buffer_size = size*nmemb;
 
  if (wt->sizeleft)
  {
    /* copy as much as possible from the source to the destination */ 
    size_t copy_this_much = wt->sizeleft;
    if(copy_this_much > buffer_size)
      copy_this_much = buffer_size;
    memcpy(dest, wt->readptr, copy_this_much);
 
    wt->readptr += copy_this_much;
    wt->sizeleft -= copy_this_much;
    return copy_this_much; /* we copied this many bytes */ 
  }
 
  return 0; /* no more data left to deliver */ 
}

static JFrameAllocator json_frame_allocator;

JFrameDocument* create_doc()
{
  return new (RawAllocator<JFrameDocument>::alloc()) JFrameDocument;
}

JFrameDocument* parse_json(JFrameDocument *doc, const char *json)
{
  doc->Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(json);
  return doc;
}

JFrameDocument* parse_json(const char *json)
{
  return parse_json(create_doc(), json);
}

JFrameDocument* json(const std::string &filename)
{
  static char readBuffer[4 << 20];

  FILE* fp = nullptr;
  fopen_s(&fp, filename.c_str(), "rb");
  if (fp)
  {
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    JFrameDocument *doc = create_doc();
    doc->ParseStream(is);
    fclose(fp);
    return doc;
  }
  return nullptr;
}

JFrameValue* http(const JFrameValue &params)
{
  // DEBUG_LOG("Url= " << params["url"].GetString());
  JFrameDocument *doc = create_doc();

  if (CURL *curl = curl_easy_init())
  {
    ReadCallbackData sendChunk;
    WriteCallbackData respChunk;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&respChunk);

    curl_easy_setopt(curl, CURLOPT_URL, params["url"].GetString());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    if (params.HasMember("verbose") && params["verbose"].GetBool())
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    if (params.HasMember("method"))
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, params["method"].GetString());

    if (params.HasMember("post"))
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params["post"].GetString());

    struct curl_slist *chunk = NULL;

    if (params.HasMember("zip"))
    {
      try
      {
        uintmax_t sz = fs::file_size(params["zip"].GetString());
        sendChunk.sizeleft = (size_t)sz;
        sendChunk.readptr = new char[sendChunk.sizeleft];

        FILE *f = nullptr;
        ::fopen_s(&f, params["zip"].GetString(), "rb");
        if (f)
        {
          ::fread_s((void*)sendChunk.readptr, (size_t)sz, 1, (size_t)sz, f);
          ::fclose(f);
        }
      }
      catch (fs::filesystem_error& e)
      {
        std::cout << e.what() << std::endl;
      }

      chunk = curl_slist_append(chunk, "Content-Type: application/zip");
      chunk = curl_slist_append(chunk, "Expect: 100-continue");

      curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&sendChunk);

      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)sendChunk.sizeleft);
    }
    else if (params.HasMember("json"))
    {
      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      params["json"].Accept(writer);
      const char *jsonString = buffer.GetString();

      chunk = curl_slist_append(chunk, "Content-Type: application/json");
      chunk = curl_slist_append(chunk, "Expect: 100-continue");

      sendChunk.sizeleft = ::strlen(jsonString);
      sendChunk.readptr = new char[sendChunk.sizeleft+1];
      sendChunk.readptr[sendChunk.sizeleft] = '\0';
      ::memcpy(sendChunk.readptr, jsonString, sendChunk.sizeleft);

      curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&sendChunk);

      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)sendChunk.sizeleft);
    }

    chunk = curl_slist_append(chunk, "User-Agent: Build Tool");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    if (params.HasMember("auth"))
    {
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      curl_easy_setopt(curl, CURLOPT_USERPWD, params["auth"].GetString());
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
      DEBUG_LOG("Failed: " << curl_easy_strerror(res));
    }
    else if (respChunk.size > 0)
    {
      parse_json(doc, respChunk.memory);
    }

    curl_slist_free_all(chunk);

    curl_easy_cleanup(curl);
  }

  return doc;
}

void copy(const std::string &from, const std::string &to)
{
  std::error_code err;
  fs::copy(from.c_str(), to.c_str(), fs::copy_options::overwrite_existing, err);
  if (err)
    std::cout << "Error: " << err.message() << std::endl;
}

void copy_with_extension(const std::string &from, const std::string &ext, const std::string &to)
{
  for (const fs::directory_entry &ent : fs::directory_iterator(from))
    if (ent.is_regular_file())
    {
      const fs::path &path = ent;
      if (path.extension() == ext)
        copy(path.string(), to);
    }
}

void copy_recursive(const std::string &from, const std::string &to)
{
  std::error_code err;
  fs::copy(from.c_str(), to.c_str(), fs::copy_options::overwrite_existing | fs::copy_options::recursive);
  if (err)
    std::cout << "Error: " << err.message() << std::endl;
}

void create_directories(const std::string &dir)
{
  fs::create_directories(dir);
}

void exec(const std::string &cmd)
{
  std::system(cmd.c_str());
}

void dump(const JFrameValue &value)
{
  if (&value == nullptr)
  {
    std::cout << "(null)" << std::endl;
    return;
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  value.Accept(writer);
  std::cout << buffer.GetString() << std::endl;
}

int main(int argc, const char *argv[])
{
  curl_global_init(CURL_GLOBAL_ALL);

  script::init();

  script::register_function("void dump(const Map@)", asFUNCTION(dump));
  script::register_function("Map@ json(string&in)", asFUNCTION(json));
  script::register_function("Map@ http(const Map@)", asFUNCTION(http));
  script::register_function("void exec(string&in)", asFUNCTION(exec));
  script::register_function("void mkdir(string&in)", asFUNCTION(create_directories));
  script::register_function("void copy(string&in, string&in)", asFUNCTION(copy));
  script::register_function("void copy(string&in, string&in, string&in)", asFUNCTION(copy_with_extension));
  script::register_function("void rcopy(string&in, string&in)", asFUNCTION(copy_recursive));

  bool res = script::build_module("build", argc > 1 ? argv[1] : "build.as", [](CScriptBuilder&, asIScriptModule &module)
  {
    if (asIScriptFunction *mainFn = script::find_function_by_decl(&module, "void main()"))
      script::call(mainFn);
  });

  script::release();

  curl_global_cleanup();

  return 0;
}