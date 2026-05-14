static const char* _default_watch_face_script = R"====(

var myWidgets = {
  bigTime: -1,
  bigDate: -1,
  sideStatus: -1,
  clock: -1,
};

function wf_on_create() {
  var tempId = -1;
  try {
    tempId = widget.create("base");
    widget.setBgColor(tempId, "#000000");
    widget.setAlign(tempId, "lv_align_top_right");
    widget.setPos(tempId, 0, 0);
    widget.setSize(tempId, 128, 168);
    widget.setBorderWidth(tempId, 0);
    widget.setRadius(tempId, 5);

    tempId = widget.create("label");
    widget.setPos(tempId, -6, 90);
    widget.setAlign(tempId, "lv_align_top_right");
    widget.setLabelFont(tempId, "RajdhaniBold48");
    widget.setLabelTextColor(tempId, "#FFFFFF");
    widget.setLabelText(tempId, "17:37");
    myWidgets.bigTime = tempId;

    tempId = widget.create("label");
    widget.setPos(tempId, -6, 135);
    widget.setAlign(tempId, "lv_align_top_right");
    widget.setLabelFont(tempId, "RajdhaniBold24");
    widget.setLabelTextColor(tempId, "#FFFFFF");
    widget.setLabelText(tempId, "10.26 SAT.");
    myWidgets.bigDate = tempId;

    tempId = widget.create("label");
    widget.setPos(tempId, 14, 3);
    widget.setRotation(tempId, 900);
    widget.setLabelFont(tempId, "RajdhaniBold16");
    widget.setLabelTextColor(tempId, "#000000");
    widget.setLabelText(tempId, "DATE: 2024.10.26 BAT: 96%");
    myWidgets.sideStatus = tempId;

    tempId = widget.create("clock");
    widget.setClockStyle(
      tempId,
      JSON.stringify({
        centerX: 94,
        centerY: 45,
        handColor: "#FFFFFF",
      })
    );
    myWidgets.clock = tempId;
  } catch (e) {
    console.error(e.message);
  }
}

function wf_on_resume() {
  wf_on_tick();
}

function wf_on_tick() {
  try {
    var now = new Date();
    var year = now.getFullYear();
    var month = now.getMonth() + 1;
    var date = now.getDate();
    var hours = now.getHours();
    var minutes = now.getMinutes();
    var seconds = now.getSeconds();
    var days = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];
    var day = days[now.getDay()] + ".";

    hours = (hours < 10 ? "0" : "") + hours;
    minutes = (minutes < 10 ? "0" : "") + minutes;
    seconds = (seconds < 10 ? "0" : "") + seconds;

    widget.setLabelText(myWidgets.bigTime, hours + ":" + minutes);
    widget.setLabelText(myWidgets.bigDate, month + "." + date + " " + day);

    var sideStatusText = year + "." + month + "." + date + " ";
    sideStatusText +=
      "BAT: " + hal.battery.percent().toFixed() + "% " + hal.battery.state();

    widget.setLabelText(myWidgets.sideStatus, sideStatusText);

    widget.updateClock(myWidgets.clock);
  } catch (e) {
    console.error(e.message);
  }
}

)====";
