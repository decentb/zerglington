#include "Zerglington.h"
using namespace BWAPI;

bool analyzed;
bool analysis_just_finished;
BWTA::Region* home;
BWTA::Region* enemy_base;

void Zerglington::onStart(){
	Broodwar->sendText("Zerglington:");
	Broodwar->sendText("Blake Bouchard and Teri Drummond");
	Broodwar->setLocalSpeed(0);
	// Enable some cheat flags
	//Broodwar->enableFlag(Flag::UserInput);
	// Uncomment to enable complete map information
	//Broodwar->enableFlag(Flag::CompleteMapInformation);

	analyzed = false;
	analysis_just_finished = false;
	foundEnemyBase = false;

	BWTA::readMap();
	Broodwar->printf("Analyzing map... this may take a minute");
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL);
	scouter.initialize();

	if (Broodwar->isReplay()){
		Broodwar->printf("The following players are in this replay:");
		for(std::set<Player*>::iterator p=Broodwar->getPlayers().begin();p!=Broodwar->getPlayers().end();p++){
			if (!(*p)->getUnits().empty() && !(*p)->isNeutral()){
				Broodwar->printf("%s, playing as a %s",(*p)->getName().c_str(),(*p)->getRace().getName().c_str());
			}
		}
	}
	else{
		Broodwar->printf("The match up is %s v %s",
			Broodwar->self()->getRace().getName().c_str(),
			Broodwar->enemy()->getRace().getName().c_str());

		Broodwar->printf("The map is %s, a %d player map",Broodwar->mapName().c_str(),Broodwar->getStartLocations().size());

		//Iterate through all units at game start
		for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++){
			//Set all workers to mine
			if ((*i)->getType().isWorker()){
				workerManager.addWorker(*i);
			}
			else if ((*i)->getType().getID() == UnitTypes::Zerg_Overlord){
				scouter.addOverlord((*i));
			}
		}
	}
}

void Zerglington::onEnd(bool isWinner){
	if (isWinner){
		//log win to file
	}
}

void Zerglington::onFrame(){
	if (show_visibility_data)
		drawVisibilityData();

	if (show_bullets)
		drawBullets();

	if (Broodwar->isReplay())
		return;

	drawStats();

	//Manage larva
	workerManager.larvaMorphing(); //Creates drones/zerglings/overlords

	workerManager.manageWorkers(); //Send workers out to do their jobs

	if (Broodwar->getFrameCount()%30==0){
		// Update Scouts
		scouter.updateScouts();
	}
	if (foundEnemyBase){
		if (!striker.initialized)
		{
			striker.initialize(scouter.getEnemyBase());
		}
		// Update Strikers
		striker.updateStrikers();

		if (striker.noMoreUnits)
		{
			foundEnemyBase = false;
			scouter.resetScouter();
		}
	}
	if (analyzed)
		drawTerrainData();
}

void Zerglington::onSendText(std::string text){
	if (text=="/show bullets"){
		show_bullets = !show_bullets;
	} else if (text=="/show players"){
		showPlayers();
	} else if (text=="/show forces"){
		showForces();
	} else if (text=="/show visibility"){
		show_visibility_data=!show_visibility_data;
	} else if (text=="/analyze"){
		Broodwar->printf("Analyzing map... this may take a minute");
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL);
}
	else{
		Broodwar->printf("You typed '%s'!",text.c_str());
		Broodwar->sendText("%s",text.c_str());
	}
}

void Zerglington::onReceiveText(BWAPI::Player* player, std::string text){
	Broodwar->printf("%s said '%s'", player->getName().c_str(), text.c_str());
}

void Zerglington::onPlayerLeft(BWAPI::Player* player){
	Broodwar->sendText("%s left the game.",player->getName().c_str());
}

void Zerglington::onNukeDetect(BWAPI::Position target){
	if (target!=Positions::Unknown)
		Broodwar->printf("Nuclear Launch Detected at (%d,%d)",target.x(),target.y());
	else
		Broodwar->printf("Nuclear Launch Detected");
}

void Zerglington::onUnitDiscover(BWAPI::Unit* unit){
	if (!Broodwar->isReplay() && Broodwar->getFrameCount()>1 && unit->getPlayer()->isEnemy(Broodwar->self()))
	{
		if (!foundEnemyBase && unit->getType().isBuilding())
		{
			// Let Scouter deal with it
			scouter.foundBuilding(unit);
		}
		striker.unitDiscovered(unit);
	}
	//Broodwar->sendText("A %s [%x] has been discovered at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

void Zerglington::onUnitEvade(BWAPI::Unit* unit){
	if (!Broodwar->isReplay() && Broodwar->getFrameCount()>1 && unit->getPlayer()->isEnemy(Broodwar->self()))
	{
		striker.unitHidden(unit);
	}
	//Broodwar->sendText("A %s [%x] was last accessible at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

void Zerglington::onUnitShow(BWAPI::Unit* unit){
	if (!Broodwar->isReplay() && Broodwar->getFrameCount()>1 && unit->getPlayer()->isEnemy(Broodwar->self()))
	{
		striker.unitShown(unit);
	}
	//Broodwar->sendText("A %s [%x] has been spotted at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

void Zerglington::onUnitHide(BWAPI::Unit* unit){
	if (!Broodwar->isReplay() && Broodwar->getFrameCount()>1 && unit->getPlayer()->isEnemy(Broodwar->self()))
	{
		striker.unitHidden(unit);
	}
	//Broodwar->sendText("A %s [%x] was last seen at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

void Zerglington::onUnitCreate(BWAPI::Unit* unit){
	if (Broodwar->getFrameCount()>1){
		if (!Broodwar->isReplay())
		{
			Broodwar->sendText("A %s [%x] has been created at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
		}
		else
		{
			/*if we are in a replay, then we will print out the build order
			(just of the buildings, not the units).*/
			if (unit->getType().isBuilding() && unit->getPlayer()->isNeutral()==false){
				int seconds=Broodwar->getFrameCount()/24;
				int minutes=seconds/60;
				seconds%=60;
				Broodwar->sendText("%.2d:%.2d: %s creates a %s",minutes,seconds,unit->getPlayer()->getName().c_str(),unit->getType().getName().c_str());
			}
		}
	}
}

void Zerglington::onUnitDestroy(BWAPI::Unit* unit){
	if (!Broodwar->isReplay() && Broodwar->getFrameCount()>1){
		Broodwar->sendText("A %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
		//If unit was our drone, remove it from our set
		
		if (unit->getPlayer() == Broodwar->self())
		{
			if (unit->getType().getID() == UnitTypes::Zerg_Drone)
			{
				workerManager.removeWorker(unit);
			}
			if (scouter.isScout(unit))
			{
				scouter.scoutKilled(unit);
			}
		}
		else if (unit->getPlayer()->isEnemy(Broodwar->self()) || striker.isStriker(unit))
		{
			striker.unitKilled(unit);
		}
	}
}

void Zerglington::onUnitMorph(BWAPI::Unit* unit){
	if (!Broodwar->isReplay())
	{
		Broodwar->sendText("A %s [%x] has been morphed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());

		//If unit was morphed to a drone:
		if(strcmp(unit->getType().getName().c_str(), "Zerg Drone") == 0){
			workerManager.addWorker(unit); //Add it to container
		}

		if (unit->getType().getID() == UnitTypes::Zerg_Overlord)
		{
			//scouter.addOverlord(unit);
		}
		else if (unit->getType().getID() == UnitTypes::Zerg_Zergling)
		{
			if (!foundEnemyBase)
				// Enemy base not yet found, pass Zergling to Scouter
				scouter.addAllZerglings();
			else
				// Enemy base found, pass Zergling to Striker
				striker.addAllZerglings();
		}
	}else{
		/*if we are in a replay, then we will print out the build order
		(just of the buildings, not the units).*/
		if (unit->getType().isBuilding() && unit->getPlayer()->isNeutral()==false){
			int seconds=Broodwar->getFrameCount()/24;
			int minutes=seconds/60;
			seconds%=60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s",minutes,seconds,unit->getPlayer()->getName().c_str(),unit->getType().getName().c_str());
		}
	}
}

void Zerglington::onUnitRenegade(BWAPI::Unit* unit){
	if (!Broodwar->isReplay())
		Broodwar->sendText("A %s [%x] is now owned by %s",unit->getType().getName().c_str(),unit,unit->getPlayer()->getName().c_str());
}

void Zerglington::onSaveGame(std::string gameName){
	Broodwar->printf("The game was saved to \"%s\".", gameName.c_str());
}

void Zerglington::drawStats(){
	std::set<Unit*> myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5,0,"I have %d units:",myUnits.size());
	std::map<UnitType, int> unitTypeCounts;
	for(std::set<Unit*>::iterator i=myUnits.begin();i!=myUnits.end();i++){
		if (unitTypeCounts.find((*i)->getType())==unitTypeCounts.end()){
			unitTypeCounts.insert(std::make_pair((*i)->getType(),0));
		}
		unitTypeCounts.find((*i)->getType())->second++;
	}
	int line=1;
	for(std::map<UnitType,int>::iterator i=unitTypeCounts.begin();i!=unitTypeCounts.end();i++){
		Broodwar->drawTextScreen(5,16*line,"- %d %ss",(*i).second, (*i).first.getName().c_str());
		line++;
	}
}

void Zerglington::drawBullets(){
	std::set<Bullet*> bullets = Broodwar->getBullets();
	for(std::set<Bullet*>::iterator i=bullets.begin();i!=bullets.end();i++){
		Position p=(*i)->getPosition();
		double velocityX = (*i)->getVelocityX();
		double velocityY = (*i)->getVelocityY();
		if ((*i)->getPlayer()==Broodwar->self()){
			Broodwar->drawLineMap(p.x(),p.y(),p.x()+(int)velocityX,p.y()+(int)velocityY,Colors::Green);
			Broodwar->drawTextMap(p.x(),p.y(),"\x07%s",(*i)->getType().getName().c_str());
		}
		else{
			Broodwar->drawLineMap(p.x(),p.y(),p.x()+(int)velocityX,p.y()+(int)velocityY,Colors::Red);
			Broodwar->drawTextMap(p.x(),p.y(),"\x06%s",(*i)->getType().getName().c_str());
		}
	}
}

void Zerglington::drawVisibilityData(){
	for(int x=0;x<Broodwar->mapWidth();x++){
		for(int y=0;y<Broodwar->mapHeight();y++){
			if (Broodwar->isExplored(x,y)){
				if (Broodwar->isVisible(x,y))
					Broodwar->drawDotMap(x*32+16,y*32+16,Colors::Green);
				else
					Broodwar->drawDotMap(x*32+16,y*32+16,Colors::Blue);
			}
			else
				Broodwar->drawDotMap(x*32+16,y*32+16,Colors::Red);
		}
	}
}

void Zerglington::showPlayers(){
	std::set<Player*> players=Broodwar->getPlayers();
	for(std::set<Player*>::iterator i=players.begin();i!=players.end();i++){
		Broodwar->printf("Player [%d]: %s is in force: %s",(*i)->getID(),(*i)->getName().c_str(), (*i)->getForce()->getName().c_str());
	}
}

void Zerglington::showForces(){
	std::set<Force*> forces=Broodwar->getForces();
	for(std::set<Force*>::iterator i=forces.begin();i!=forces.end();i++){
		std::set<Player*> players=(*i)->getPlayers();
		Broodwar->printf("Force %s has the following players:",(*i)->getName().c_str());
		for(std::set<Player*>::iterator j=players.begin();j!=players.end();j++){
			Broodwar->printf("  - Player [%d]: %s",(*j)->getID(),(*j)->getName().c_str());
		}
	}
}

// BWTA FUNCTIONS
DWORD WINAPI AnalyzeThread(){
	BWTA::analyze();

	//self start location only available if the map has base locations
	if (BWTA::getStartLocation(Broodwar->self())!=NULL){
		home       = BWTA::getStartLocation(Broodwar->self())->getRegion();
	}

	analyzed   = true;
	analysis_just_finished = true;
	Broodwar->sendText("Terrain analysis complete");
	return 0;
}


void Zerglington::drawTerrainData(){
	//we will iterate through all the base locations, and draw their outlines.
	for(std::set<BWTA::BaseLocation*>::const_iterator i=BWTA::getBaseLocations().begin();i!=BWTA::getBaseLocations().end();i++){
		TilePosition p=(*i)->getTilePosition();
		Position c=(*i)->getPosition();

		//draw outline of center location
		Broodwar->drawBox(CoordinateType::Map,p.x()*32,p.y()*32,p.x()*32+4*32,p.y()*32+3*32,Colors::Blue,false);

		//draw a circle at each mineral patch
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getStaticMinerals().begin();j!=(*i)->getStaticMinerals().end();j++){
			Position q=(*j)->getInitialPosition();
			Broodwar->drawCircle(CoordinateType::Map,q.x(),q.y(),30,Colors::Cyan,false);
		}

		//draw the outlines of vespene geysers
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getGeysers().begin();j!=(*i)->getGeysers().end();j++){
			TilePosition q=(*j)->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x()*32,q.y()*32,q.x()*32+4*32,q.y()*32+2*32,Colors::Orange,false);
		}

		//if this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
			Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),80,Colors::Yellow,false);
	}

	//we will iterate through all the regions and draw the polygon outline of it in green.
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++){
		BWTA::Polygon p=(*r)->getPolygon();
		for(int j=0;j<(int)p.size();j++)
		{
			Position point1=p[j];
			Position point2=p[(j+1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Green);
		}
	}

	//we will visualize the chokepoints with red lines
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++){
		for(std::set<BWTA::Chokepoint*>::const_iterator c=(*r)->getChokepoints().begin();c!=(*r)->getChokepoints().end();c++){
			Position point1=(*c)->getSides().first;
			Position point2=(*c)->getSides().second;
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Red);
		}
	}
}