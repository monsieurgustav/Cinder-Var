# Cinder-Var
A basic-live-cinder-json-saved-variable system using _Simon Geilfus'_ Watchdog as a submodule.

```
ci::Var<float> mRadius{ 1.0f, "radius };
```
In `assets/`, the parameter values are stored as follows in the _live_vars.json_ file.

```
{
   "default" : {
      "radius" : "1.0",
   }
}
```

Editing and saving that file will update the value in real-time.
