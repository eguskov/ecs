## Settings
There are two files with settings: **build.json** and **secret.json**

### secret.json
**Never commit** this file to a public repository!

It is better to put it in .gitignore.

[Follow the link to create a new token](https://github.com/settings/tokens)

```json
{
  "username": "bob",
  "token": "<TOKEN>"
}
```

### build.json
Just set any **tag** and it will be created by GitHub.
```json
{
  "repo": "my-cool-repo",
  "tag": "v0.0.2",
  "body": "",
  "draft": false,
  "prerelease": true
}
```

## Script
By default it is **build.as**, but it could be passed as an argument to **build.exe**

### Script functions
Map@ and Array@ this a json-like object and can be easily cast one to another.

Each of them have a method isArray() and isMap to determine which interface must be used
to access the data.

```
Map@ m;
Array@ a = m;

/* And vice versa */
```

#### Map
```
Map@ m = {
  {"k1", "v1"},
  {"k2", "v2"},
  {"k3", "v3"},
  {"k4", "v4"},
  {"k5", Map = {{"k10", "v10"}, {"k11", "v11"}},
  {"k6", Array = {0, 1, 2, 3}}
};
```
* Map@ set(string&in key, bool value)
* Map@ set(string&in key, int value)
* Map@ set(string&in key, float value)
* Map@ set(string&in key, string&in value)
* Map@ set(string&in key, Map@ value)
* Map@ set(string&in key, Array@ value)
* bool getBool(string&in key)
* int getInt(string&in key)
* float getFloat(string&in key)
* string getString(string&in key)
* Map@ getMap(string&in key)
* Array@ getArray(string&in key)

#### Array
```
Array@ arr = {0, 1, 2, 3};
```
* Array@ push(bool item)
* Array@ push(int item)
* Array@ push(float item)
* Array@ push(string&in item)
* Array@ push(Map@ item)
* Array@ push(Array@ item)
* int size()
* bool getBool(int index)
* int getInt(int index)
* float getFloat(int index)
* string getString(int index)
* Map@ getMap(int index)
* Array@ getArray(int index)

#### Globals
* void dump(const Map@ map)
* Map@ json(string&in path_to_file)
* Map@ http(const Map@ params)
  * params: url - string
  * params: verbose - bool
  * params: method - string. This custom method DELETE, PATCH etc.
  * params: post - string. Post fields key=val&a=b
  * params: zip - string. Upload zip-file as POST
  * params: json - Map. Pass map as json
  * params: auth - string. Basic authentication login:pass
* void exec(string&in command)
  * Execute any system command. For example, exec("ls -la");
* void mkdir(string&in directories)
  * Create full path. mkrdir("a/b/c") will create a and b and c directories
* void copy(string&in from, string&in to)
* void copy(string&in from, string&in extension, string&in, to)
* void rcopy(string&in from, string&in to)
  * Recursive copy