const string USERNAME = "eguskov";
const string TOKEN = "fb53f645c0b632a9e7e510230e814f6493c24c39";
const string REPO = "ecs";
const string TAG = "v0.0.1";

Map@ api_call(string&in method)
{
  print("Call: "+"https://api.github.com/"+method);
  return http_post(Map = {
    {"url", "https://api.github.com/"+method},
    {"auth", USERNAME+":"+TOKEN}
  });
}

Map@ find_release(Array@ releases)
{
  for (int i = 0; i < releases.size(); ++i)
    if (releases.getMap(i).getString("tag_name") == TAG)
      return releases.getMap(i);
  return null;
}

void main()
{
  exec("msbuild /property:GenerateFullPaths=true /p:Configuration=Release /p:Platform=\"x86\" /t:build ../ecs.sln");

  mkdir("../build/bin/");

  copy("../Release/jemalloc.dll", "../build/bin/");
  copy("../Release/sample.exe", "../build/bin/");

  copy("../sample/", ".as", "../build/bin/");
  copy("../sample/", ".json", "../build/bin/");
  copy("../sample/editor.cmd", "../build/bin/");

  rcopy("../assets", "../build/assets");
  rcopy("../webui/dist", "../build/bin/editor");

  Array@ releases = api_call("repos/"+USERNAME+"/"+REPO+"/releases");
  // dump(releases);

  Map@ release = find_release(releases);
  // dump(release);

  if (release is null)
  {
    print("Upload new release with tag: "+TAG);
  }
  else
  {
    print("Update existing release with tag: "+TAG);
  }
}