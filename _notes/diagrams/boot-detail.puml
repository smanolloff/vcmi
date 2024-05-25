@startuml "vcmi-1.32-boot"

skinparam defaultTextAlignment center

title "VCMI-1.3.2 boot process"

' use "w"s to stretch image (VS code preview does not show full diagram)
title "VCMI-1.3.2 boot process wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww"

!procedure $node3($txt, $file, $line)
  :$txt

  <font color="gray">$file:$line</font>;
!endprocedure

!procedure $node4($txt, $fun, $file, $line)
  :$txt

  <font color="gray">$file:$line ""($fun)""</font>;
!endprocedure


start

$node4("Program start", "main", "CGameHandler.cpp", 1060)
$node4("Load <font:monospaced>config/settings.json</font>", "SettingsStorage::init", "CConfigHandler.cpp", 57)
$node4("Open socket for clients connections", "CVCMIServer::startAsyncAccept", "CVMIServer.cpp", 339)
split
  '$node4("Wait until game starts", "", "CVMIServer.cpp", 200)
  while (state == LOBBY or GAMEPLAY_STARTING)
    $node3("sleep 50ms", "CVMIServer.cpp", 200)
  endwhile
split again
  $node4("state = GAMEPLAY_STARTING", "CVCMIServer::prepareToStartGame", "CVMIServer.cpp", 299)
  detach
end split
$node3("Start game", "CVMIServer.cpp", 207)
stop
@enduml
