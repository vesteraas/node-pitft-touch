{
  "targets": [
    {
      "target_name": "ts",
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "sources": [ "src/touchscreen.cc" ]
    }
  ]
}
