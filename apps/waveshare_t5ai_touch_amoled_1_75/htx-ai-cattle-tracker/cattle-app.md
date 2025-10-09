The hardware is 466x466 pixel oled screen. With circular screen, so effective area should be in a circular area. Touch is enabled.
Currently I dont have hardware available. Create dummy interface for later implementation.

Re-invent the current display UIs and convert to a tracker app UI.

LVGL Todos: Three Main Screens:
1. Idle Screen：idel circle that has text at center.
2. Tracking Screen：Active compass that takes in 3 axis angle and Calibreate process if not calibraited. And message to user to do 8 pattern calibreation. Once it's done the compass can rotate base on the target lon/lat of target data. The center of the compass is the tracker's it's own gps data. Again Keep the gps data dummy. And draw the compass with simple generic lvgl components for now. 
3. When slide from top(touch gesture) then a setting menu will show. GPS num status, time date and volume slide bar. 
4. sos screen: when the button is pressed long enough 3 seconds for detect a small animation on screen. And animation is 3s to invoke a SOS state. Hold for. There's an x button on the screen to cancel the SOS state alert. 

Focus on the UI and UX first. Dont bother too much to implement the hardware for now.









