#include <PhySim.hpp>

int gravityVal = -2;

namespace physim{

	/********************************* function: constructor ***********************************************
	*******************************************************************************************************/

	PhySim::PhySim(std::vector< float> tableParams){
		btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
		btCollisionDispatcher* dispatcher = new	btCollisionDispatcher(collisionConfiguration);
		btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
		btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
		dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,overlappingPairCache,solver,collisionConfiguration);
		dynamicsWorld->setGravity(btVector3(0,0,gravityVal));
	}

	/********************************* function: addTable **************************************************
	*******************************************************************************************************/

	void PhySim::addTable(std::vector< float> tableParams){
		btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(0.40),btScalar(0.40),btScalar(0.20)));

		btTransform groundTransform;
		groundTransform.setIdentity();

		groundTransform.setOrigin(btVector3(tableParams[3], tableParams[7], tableParams[11]));

		btMatrix3x3 rotMat;
		rotMat.setValue(tableParams[0], tableParams[1], tableParams[2], 
						tableParams[4], tableParams[5], tableParams[6], 
						tableParams[8], tableParams[9], tableParams[10]);
		groundTransform.setBasis(rotMat);

		btScalar mass(0.);
		bool isDynamic = (mass != 0.f);
		btVector3 localInertia(0,0,0);
		if (isDynamic)
			groundShape->calculateLocalInertia(mass,localInertia);

		btRigidBody* body = new btRigidBody(mass,0,groundShape,localInertia);	
		body->setWorldTransform(groundTransform);
		body->setDamping(0.99f,0.99f);
		body->setFriction(1.f);
		body->setRestitution(0.f);
		dynamicsWorld->addRigidBody(body);
	}

	/********************************* function: initRigidBodies *******************************************
	*******************************************************************************************************/

	void PhySim::initRigidBody(std::string objName){
		std::string sfileName = "/home/chaitanya/github/PhysimGlobalPose/models/" + objName + "/" + objName + ".obj";
		const char* cfileName = sfileName.c_str();
		GLInstanceGraphicsShape* glmesh = LoadMeshFromObj(cfileName, "");
		std::cout << "[INFO] Obj loaded: Extracted "<< glmesh->m_numvertices << " vertices from obj file " << sfileName << std::endl;

		const GLInstanceVertex& v = glmesh->m_vertices->at(0);
		btConvexHullShape* shape = new btConvexHullShape((const btScalar*)(&(v.xyzw[0])), glmesh->m_numvertices, sizeof(GLInstanceVertex));
		float scaling[4] = {1,1,1,1};
		btVector3 localScaling(scaling[0],scaling[1],scaling[2]);
		shape->setLocalScaling(localScaling);
		shape->setMargin(0.001);

		btScalar mass(10.f);
		bool isDynamic = (mass != 0.f);
		btVector3 localInertia(0,0,0);
		if (isDynamic)
			shape->calculateLocalInertia(mass,localInertia);

		btRigidBody* body = new btRigidBody(mass,0,shape,localInertia);
		body->setDamping(0.99f,0.99f);
		body->setFriction(1.f);
		body->setRestitution(0.f);
		body->setActivationState(DISABLE_DEACTIVATION);
		rBodyMap[objName] = body;
		cShapes[objName] = shape;
	}

	/********************************* function: addObjects ************************************************
	*******************************************************************************************************/

	void PhySim::addObject(std::string objName, Eigen::Isometry3d tform, float mass){
		Eigen::Vector3d trans = tform.translation();
		Eigen::Quaterniond rot(tform.rotation());
		btVector3 position(trans[0], trans[1], trans[2]);
		btQuaternion quat(rot.x(), rot.y(), rot.z(), rot.w());

		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(position);
		startTransform.setRotation(quat);
			
		rBodyMap[objName]->setWorldTransform(startTransform);

		bool isDynamic = (mass != 0.f);
		btVector3 localInertia(0,0,0);
		if (isDynamic)
			cShapes[objName]->calculateLocalInertia(mass,localInertia);

		rBodyMap[objName]->setMassProps(mass, localInertia);
	    dynamicsWorld->addRigidBody(rBodyMap[objName]);
	}

	/********************************* function: simulate **************************************************
	*******************************************************************************************************/

	void PhySim::simulate(int num_steps){
		dynamicsWorld->setGravity(btVector3(0,0,gravityVal));
		for (int ii = 0; ii < num_steps; ii++)
			dynamicsWorld->stepSimulation(1.f/60.f);
	}

	/********************************* function: getTransform **********************************************
	*******************************************************************************************************/

	void PhySim::getTransform(std::string objName, Eigen::Isometry3d &tform){
		btTransform btform = rBodyMap[objName]->getWorldTransform();
		btQuaternion quatBullet = btform.getRotation();
		btVector3 transBullet = btform.getOrigin();
		Eigen::Matrix3d rotEig = Eigen::Quaterniond(quatBullet.w(),quatBullet.x(),quatBullet.y(),quatBullet.z()).toRotationMatrix();
		Eigen::Vector3d transEig;
		transEig << transBullet.getX(), transBullet.getY(), transBullet.getZ();
		tform.setIdentity();
		tform = tform*rotEig;
		tform.translation() = transEig;
	}

	/********************************* function: removeObject **********************************************
	*******************************************************************************************************/

	void PhySim::removeObject(std::string objName){
		dynamicsWorld->removeRigidBody(rBodyMap[objName]);
		rBodyMap[objName]->clearForces();
		btVector3 zeroVector(0,0,0);
		rBodyMap[objName]->setLinearVelocity(zeroVector);
		rBodyMap[objName]->setAngularVelocity(zeroVector);
	}

	/********************************* function: destructor **********************************************
	*******************************************************************************************************/

	PhySim::~PhySim(){
		// deleting collision shapes
		for (std::map<std::string, btCollisionShape*>::iterator it=cShapes.begin(); it!=cShapes.end(); ++it){
			btCollisionShape* shape = it->second;
			delete shape;
		}

		// deleting rigid bodies
		for (std::map<std::string, btRigidBody*>::iterator it=rBodyMap.begin(); it!=rBodyMap.end(); ++it){
			btRigidBody* body = it->second;
			delete body;
		}
		// deleting the table
		for (int i=dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--) {
			btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(obj);
			dynamicsWorld->removeCollisionObject(obj);
			delete obj;
		}
	}

	/********************************* end of functions ****************************************************
	*******************************************************************************************************/
}