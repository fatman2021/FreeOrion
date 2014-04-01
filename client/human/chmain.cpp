#ifdef OGRE_STATIC_LIB
#include "OgrePlugins/OgreOctreePlugin.h"
#include "OgrePlugins/OgreParticleFXPlugin.h"
#include "OgrePlugins/OgreGLPlugin.h"
#endif
#include <GG/Ogre/Plugins/OISInput.h>

#include "HumanClientApp.h"
#include "../../parse/Parse.h"
#include "../../util/OptionsDB.h"
#include "../../util/Directories.h"
#include "../../util/Logger.h"
#include "../../util/Version.h"
#include "../../util/XMLDoc.h"
#include "../../util/i18n.h"
#include "../../UI/EntityRenderer.h"
#include "../../UI/Hotkeys.h"

#include <OgreCamera.h>
#include <OgreLogManager.h>
#include <OgreRenderSystem.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreViewport.h>

#include <GG/utf8/checked.h>

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>

#include "chmain.h"


// The STORE_FULLSCREEN_FLAG parameter below controls whether the fullscreen
// option is stored in the XML config file.  On Win32 it is not, because the
// installed version of FO is run with the command-line flag added in as
// appropriate.
#ifdef FREEORION_WIN32
const bool  STORE_FULLSCREEN_FLAG = false;
#else
const bool  STORE_FULLSCREEN_FLAG = true;
#endif


#if defined(FREEORION_LINUX) || defined(FREEORION_FREEBSD)
int main(int argc, char* argv[]) {
    // copy command line arguments to vector
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
        args.push_back(argv[i]);

    // set options from command line or config.xml, or generate config.xml
    if (mainConfigOptionsSetup(args) != 0) {
        std::cerr << "main() failed config." << std::endl;
        return 1;
    }
#endif
#ifdef FREEORION_WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    // copy UTF-16 command line arguments to UTF-8 vector
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        std::wstring argi16(argv[i]);
        std::string argi8;
        utf8::utf16to8(argi16.begin(), argi16.end(), std::back_inserter(argi8));
        args.push_back(argi8);
    }

    // set options from command line or config.xml, or generate config.xml
    if (mainConfigOptionsSetup(args) != 0) {
        std::cerr << "main() failed config." << std::endl;
        return 1;
    }
#endif
#ifndef FREEORION_MACOSX
    // did the player request help output?
    if (GetOptionsDB().Get<bool>("help")) {
        GetOptionsDB().GetUsage(std::cerr);
        return 0;   // quit without actually starting game
    }

    // set up rendering and run game
    if (mainSetupAndRunOgre() != 0) {
        std::cerr << "main() failed to setup or run ogre." << std::endl;
        return 1;
    }
    return 0;
}
#endif


int mainConfigOptionsSetup(const std::vector<std::string>& args) {
    InitDirs((args.empty() ? "" : *args.begin()));

    // read and process command-line arguments, if any
    try {
        // add entries in options DB that have no other obvious place
        GetOptionsDB().AddFlag('h', "help",                 UserStringNop("OPTIONS_DB_HELP"),                  false);
        GetOptionsDB().AddFlag('g', "generate-config-xml",  UserStringNop("OPTIONS_DB_GENERATE_CONFIG_XML"),   false);
        GetOptionsDB().AddFlag('f', "fullscreen",           UserStringNop("OPTIONS_DB_FULLSCREEN"),            STORE_FULLSCREEN_FLAG);
        GetOptionsDB().Add("reset-fullscreen-size",         UserStringNop("OPTIONS_DB_RESET_FSSIZE"),          true);
        GetOptionsDB().Add<int>("fullscreen-monitor-id",    UserStringNop("OPTIONS_DB_FULLSCREEN_MONITOR_ID"), 0, RangedValidator<int>(0, 5));
        GetOptionsDB().AddFlag('q', "quickstart",           UserStringNop("OPTIONS_DB_QUICKSTART"),            false);
        GetOptionsDB().AddFlag("auto-advance-first-turn",   UserStringNop("OPTIONS_DB_AUTO_FIRST_TURN"),       false);
        GetOptionsDB().Add<std::string>("load",             UserStringNop("OPTIONS_DB_LOAD"),                  "", Validator<std::string>(), false);
        GetOptionsDB().Add("UI.sound.music-enabled",        UserStringNop("OPTIONS_DB_MUSIC_ON"),              true);
        GetOptionsDB().Add("UI.sound.enabled",              UserStringNop("OPTIONS_DB_SOUND_ON"),              true);
        GetOptionsDB().Add<std::string>("version-string",   UserStringNop("OPTIONS_DB_VERSION_STRING"),
                                        FreeOrionVersionString(),   Validator<std::string>(),                  true);
        GetOptionsDB().AddFlag('r', "render-simple",        UserStringNop("OPTIONS_DB_RENDER_SIMPLE"),         false);

        // Add the keyboard shortcuts
        Hotkey::AddOptions(GetOptionsDB());


        // read config.xml and set options entries from it, if present
        {
            XMLDoc doc;
            try {
                boost::filesystem::ifstream ifs(GetConfigPath());
                if (ifs) {
                    doc.ReadDoc(ifs);
                    // reject config files from out-of-date version
                    if (doc.root_node.ContainsChild("version-string") &&
                        doc.root_node.Child("version-string").Text() == FreeOrionVersionString())
                    {
                        GetOptionsDB().SetFromXML(doc);
                    }
                }
            } catch (const std::exception&) {
                std::cerr << UserString("UNABLE_TO_READ_CONFIG_XML") << std::endl;
            }
        }


        // override previously-saved and default options with command line parameters and flags
        GetOptionsDB().SetFromCommandLine(args);


        // Handle the case where the resource-dir does not exist anymore
        // gracefully by resetting it to the standard path into the
        // application bundle.  This may happen if a previous installed
        // version of FreeOrion was residing in a different directory.
        if (!boost::filesystem::exists(GetResourceDir()) ||
            !boost::filesystem::exists(GetResourceDir() / "credits.xml") ||
            !boost::filesystem::exists(GetResourceDir() / "data" / "art" / "misc" / "missing.png"))
        {
            Logger().debugStream() << "Resources directory from config.xml missing or does not contain expected files. Resetting to default.";

            GetOptionsDB().Set<std::string>("resource-dir", "");

            // double-check that resetting actually fixed things...
            if (!boost::filesystem::exists(GetResourceDir()) ||
                !boost::filesystem::exists(GetResourceDir() / "credits.xml") ||
                !boost::filesystem::exists(GetResourceDir() / "data" / "art" / "misc" / "missing.png"))
            {
                Logger().debugStream() << "Default Resources directory missing or does not contain expected files. Cannot start game.";

                std::string path_string;
#if defined(FREEORION_WIN32)
                boost::filesystem::path::string_type path_string_native = GetResourceDir().native();
                utf8::utf16to8(path_string_native.begin(), path_string_native.end(), std::back_inserter(path_string));
#else
                path_string = GetResourceDir().string();
#endif
                throw std::runtime_error("Unable to load game resources at default location: " +
                                         path_string + " : Install may be broken.");
            }
        }


        // did the player request generation of config.xml, saving the default (or current) options to disk?
        if (GetOptionsDB().Get<bool>("generate-config-xml")) {
            try {
                boost::filesystem::ofstream ofs(GetConfigPath());
                if (ofs) {
                    GetOptionsDB().GetXML().WriteDoc(ofs);
                } else {
                    std::cerr << UserString("UNABLE_TO_WRITE_CONFIG_XML") << std::endl;
#if defined(FREEORION_WIN32)
                    boost::filesystem::path::string_type path_string_native = GetConfigPath().native();
                    std::string path_string;
                    utf8::utf16to8(path_string_native.begin(), path_string_native.end(), std::back_inserter(path_string));
                    std::cerr << path_string << std::endl;
#else
                    std::cerr << GetConfigPath().string() << std::endl;
#endif
                }
            } catch (const std::exception&) {
                std::cerr << UserString("UNABLE_TO_WRITE_CONFIG_XML") << std::endl;
            }
        }

        if (GetOptionsDB().Get<bool>("render-simple")) {
            GetOptionsDB().Set<bool>("UI.galaxy-gas-background",false);
            GetOptionsDB().Set<bool>("UI.galaxy-starfields",    false);
            GetOptionsDB().Set<bool>("show-fps",                true);
        }

    } catch (const std::invalid_argument& e) {
        std::cerr << "main() caught exception(std::invalid_argument): " << e.what() << std::endl;
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "main() caught exception(std::runtime_error): " << e.what() << std::endl;
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "main() caught exception(std::exception): " << e.what() << std::endl;
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        return 1;
    } catch (...) {
        std::cerr << "main() caught unknown exception." << std::endl;
        return 1;
    }

    return 0;
}


int mainSetupAndRunOgre() {
    Ogre::LogManager*       log_manager = 0;
    Ogre::Root*             root = 0;
    OISInput*               ois_input_plugin = 0;
#ifdef OGRE_STATIC_LIB
    Ogre::OctreePlugin*     octree_plugin = 0;
    Ogre::ParticleFXPlugin* particle_fx_plugin = 0;
    Ogre::GLPlugin*         gl_plugin = 0;
#endif

    try {
        using namespace Ogre;
        log_manager = new LogManager();

#ifndef FREEORION_WIN32
        log_manager->createLog((GetUserDir() / "ogre.log").string(), true, false);
        root = new Root((GetRootDataDir() / "ogre_plugins.cfg").string());
        // this line is needed on some Linux systems which otherwise will crash with
        // errors about GLX_icon.png being missing.
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation((ClientUI::ArtDir() / ".").string(),
                                                                       "FileSystem", "General");
#else
        boost::filesystem::path::string_type file_native = (GetUserDir() / "ogre.log").native();
        std::string ogre_log_file;
        utf8::utf16to8(file_native.begin(), file_native.end(), std::back_inserter(ogre_log_file));
        log_manager->createLog(ogre_log_file, true, false);

        // for some reason, for this file, the built-in path conversion seems to work properly
        std::string plugins_cfg_file = (GetRootDataDir() / "ogre_plugins.cfg").string();
        root = new Root(plugins_cfg_file);
#endif

#if defined(OGRE_STATIC_LIB)
        octree_plugin = new Ogre::OctreePlugin;
        particle_fx_plugin = new Ogre::ParticleFXPlugin;
        gl_plugin = new Ogre::GLPlugin;
        root->installPlugin(octree_plugin);
        root->installPlugin(particle_fx_plugin);
        root->installPlugin(gl_plugin);
#endif


        RenderSystem* selected_render_system = root->getRenderSystemByName("OpenGL Rendering Subsystem");
        if (!selected_render_system)
            throw std::runtime_error("Failed to find an Ogre GL render system.");

        root->setRenderSystem(selected_render_system);

        int colour_depth = GetOptionsDB().Get<int>("color-depth");
        bool fullscreen = GetOptionsDB().Get<bool>("fullscreen");
        std::pair<int, int> width_height = HumanClientApp::GetWindowWidthHeight(selected_render_system);
        int width(width_height.first), height(width_height.second);
        std::pair<int, int> left_top = HumanClientApp::GetWindowLeftTop();
        int left(left_top.first), top(left_top.second);

        root->initialise(false);

        Ogre::NameValuePairList misc_window_params;
        misc_window_params["title"] = "FreeOrion " + FreeOrionVersionString();
        misc_window_params["colourDepth"] = boost::lexical_cast<std::string>(colour_depth);
        misc_window_params["left"] = boost::lexical_cast<std::string>(left);
        misc_window_params["top"] = boost::lexical_cast<std::string>(top);
        misc_window_params["macAPI"] = "carbon";
        misc_window_params["border"] = "resize";
        if (fullscreen)
            misc_window_params["monitorIndex"] = boost::lexical_cast<std::string>(
                GetOptionsDB().Get<int>("fullscreen-monitor-id"));

        RenderWindow* window = root->createRenderWindow("FreeOrion " + FreeOrionVersionString(),
                                                        width, height, fullscreen, &misc_window_params);

#ifdef FREEORION_WIN32
#  ifdef IDI_ICON1
        // set window icon to embedded application icon
        HWND hwnd;
        window->getCustomAttribute("WINDOW", &hwnd);
        HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);
        SetClassLong (hwnd, GCL_HICON,
            (LONG)LoadIcon (hInst, MAKEINTRESOURCE (IDI_ICON1)));
#  endif
#endif

        SceneManager* scene_manager = root->createSceneManager("OctreeSceneManager", "SceneMgr");

        Camera* camera = scene_manager->createCamera("Camera");

        camera->setPosition(Vector3(0, 0, 500));    // Position it at 500 in Z direction
        camera->lookAt(Vector3(0, 0, -300));        // Look back along -Z
        camera->setNearClipDistance(5);

        Viewport* viewport = window->addViewport(camera);
        viewport->setBackgroundColour(ColourValue(0, 0, 0));

        //EntityRenderer entity_renderer(scene_manager);

        parse::init();
        HumanClientApp app(root, window, scene_manager, camera, viewport, GetRootDataDir() / "OISInput.cfg");


        ois_input_plugin = new OISInput;
        ois_input_plugin->SetRenderWindow(window);
        root->installPlugin(ois_input_plugin);


        if (GetOptionsDB().Get<bool>("quickstart")) {
            // immediately start the server, establish network connections, and
            // go into a single player game, using default universe options (a
            // standard quickstart, without requiring the user to click the
            // quickstart button).
            app.NewSinglePlayerGame(true);  // acceptable to call before app()
        }

        std::string load_filename = GetOptionsDB().Get<std::string>("load");
        if (!load_filename.empty()) {
            // immediately start the server, establish network connections, and
            // go into a single player game, loading the indicated file
            // (without requiring the user to click the load button).
            app.LoadSinglePlayerGame(load_filename);  // acceptable to call before app()
        }

        // run rendering loop
        app();  // calls GUI::operator() which calls OgreGUI::Run() which starts rendering loop

    } catch (const HumanClientApp::CleanQuit&) {
        // do nothing
        std::cout << "mainSetupAndRunOgre caught CleanQuit" << std::endl;
    } catch (const std::invalid_argument& e) {
        Logger().errorStream() << "main() caught exception(std::invalid_argument): " << e.what();
        std::cerr << "main() caught exception(std::invalid_arg): " << e.what() << std::endl;
    } catch (const std::runtime_error& e) {
        Logger().errorStream() << "main() caught exception(std::runtime_error): " << e.what();
        std::cerr << "main() caught exception(std::runtime_error): " << e.what() << std::endl;
    } catch (const  boost::io::format_error& e) {
        Logger().errorStream() << "main() caught exception(boost::io::format_error): " << e.what();
        std::cerr << "main() caught exception(boost::io::format_error): " << e.what() << std::endl;
    } catch (const GG::ExceptionBase& e) {
        Logger().errorStream() << "main() caught exception(" << e.type() << "): " << e.what();
        std::cerr << "main() caught exception(" << e.type() << "): " << e.what() << std::endl;
    } catch (const std::exception& e) {
        Logger().errorStream() << "main() caught exception(std::exception): " << e.what();
        std::cerr << "main() caught exception(std::exception): " << e.what() << std::endl;
    } catch (...) {
        Logger().errorStream() << "main() caught unknown exception.";
        std::cerr << "main() caught unknown exception." << std::endl;
    }

    if (root) {
        root->uninstallPlugin(ois_input_plugin);
        delete ois_input_plugin;

#ifdef OGRE_STATIC_LIB
        root->uninstallPlugin(octree_plugin);
        root->uninstallPlugin(particle_fx_plugin);
        root->uninstallPlugin(gl_plugin);
        delete octree_plugin;
        delete particle_fx_plugin;
        delete gl_plugin;
#endif

        delete root;
    }

    if (log_manager)
        delete log_manager;

    return 0;
}
