var timerStart = true;

function myTimer(d0, timeInSeconds)
{
   var d=(new Date()).valueOf();
   var diff = d-d0;
   var minutes = Math.floor(diff/1000/60);
   var seconds = Math.floor(diff/1000)-minutes*60;
   postMessage(seconds);
   return seconds;
}
               
if (timerStart){
   var d0=(new Date()).valueOf();
   var timeInSeconds;
   myVar = setInterval(function(){
        self.onmessage = function(e) {
            timeInSeconds = e.data;
        }
        let seconds = myTimer(d0, timeInSeconds);
        if (seconds == timeInSeconds || seconds > timeInSeconds) {
            d0 = (new Date()).valueOf();
        }
    },100);
   timerStart = false;
}