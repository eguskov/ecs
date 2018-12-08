Map@ settings = json("build.json");
Map@ secret = json("secret.json");

Map@ api(Map@ params, string&in method)
{
  print("Call: "+"https://api.github.com/"+method);

  params.set("url", "https://api.github.com/"+method);
  params.set("auth", secret.getString("username")+":"+secret.getString("token"));

  return http(params);
}

Map@ api_releases(Map@ params, string&in method = "")
{
  string url = "repos/"+secret.getString("username")+"/"+settings.getString("repo")+"/releases";
  if (method != "")
    url += "/"+method;
  return api(params, url);
}

Map@ upload(Map@ params, string&in method)
{
  print("Call: "+"https://uploads.github.com/"+method);

  params.set("url", "https://uploads.github.com/"+method);
  params.set("auth", secret.getString("username")+":"+secret.getString("token"));

  return http(params);
}

Map@ upload_releases(Map@ params, string&in method = "")
{
  string url = "repos/"+secret.getString("username")+"/"+settings.getString("repo")+"/releases";
  if (method != "")
    url += "/"+method;
  return upload(params, url);
}

Map@ find_release(Array@ releases)
{
  for (int i = 0; i < releases.size(); ++i)
    if (releases.getMap(i).getString("tag_name") == settings.getString("tag"))
      return releases.getMap(i);
  return null;
}

Map@ find_asset(string&in name, Array@ assets)
{
  for (int i = 0; i < assets.size(); ++i)
    if (assets.getMap(i).getString("name") == name)
      return assets.getMap(i);
  return null;
}

void zip(string&in input, string&in output)
{
  exec("powershell Compress-Archive -Force -Path "+input+" -DestinationPath "+output);
}

void create_build()
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

  zip("../build", "build.zip");
}

void main()
{
  create_build();

  Map@ release = find_release(api_releases(Map = {}));

  if (release is null)
  {
    print("Upload new release with tag: "+settings.getString("tag"));

    @release = api_releases(Map = {{"json", Map = {
      {"tag_name", settings.getString("tag")},
      {"target_commitish", "master"},
      {"name", "Sample "+settings.getString("tag")},
      {"body", ""},
      {"draft", false},
      {"prerelease", true}
    }}});
    dump(release);
  }
  else
    print("Update existing release with tag: "+settings.getString("tag"));

  string buildName = "build.zip";

  Map@ asset = find_asset(buildName, release.getArray("assets"));

  if (asset !is null)
  {
    print("Delete an old build: "+buildName);

    Map@ resp = api_releases(Map = {{"method", "DELETE"}}, "assets/"+asset.getInt("id"));
    dump(resp);
  }

  print("Upload build: "+buildName);
  upload_releases(Map = {{"zip", "build.zip"}}, ""+release.getInt("id")+"/assets?name="+buildName);
}