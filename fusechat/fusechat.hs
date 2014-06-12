--
-- Copyright (c) 2012 Citrix Systems, Inc.
-- 
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--

{-# LANGUAGE Arrows, ScopedTypeVariables, RankNTypes, OverloadedStrings #-}
module Main where

import Data.Map (Map)
import Data.Maybe
import qualified Control.Exception as E
import Control.Applicative
import Control.Concurrent
import Control.Monad
import System.Posix.Syslog
import qualified Data.Map as Map

import Tools.Log
import Tools.Db

import Curl
import Protocol
import Rpc
import Rpc.Autogen.FusechatServer
import Errors
import App

main :: IO ()
main
  = withCurlDo . withSyslog "fusechat" [] USER $ run
  where
    run = do
      rpc_context <- rpcConnectTo (DomainBus 0)
      app_context <- newAppContext
      errors =<< ( rpc rpc_context $ rpcRunService "com.citrix.xenclient.fusechat" $ runApp app_context run' )

    run' = do
      rpcExpose "/" (interfaces implementation)
      forever (liftIO (threadDelay maxBound))
    errors (Left ex) = fatal (show ex)
    errors _ = return ()

implementation :: FusechatServer App
implementation =
  FusechatServer
  {
    comCitrixXenclientFusechatListDesktops = \credentials -> listDesktops (mkCredentials credentials) >>= return . map exportApp
  , comCitrixXenclientFusechatGetLaunchRef = \credentials app -> getLaunchRef (mkCredentials credentials) (Resource app) >>= return . exportLauncher
  , comCitrixXenclientFusechatGetServerUrl = fromMaybe "" <$> serverURL
  , comCitrixXenclientFusechatSetServerUrl = \url -> dbMaybeWrite "/fusechat/url" ( if url == "" then Nothing else Just url )
  , comCitrixXenclientFusechatGetPnagentPath = pnAgentPath
  , comCitrixXenclientFusechatSetPnagentPath = \path -> dbMaybeWrite "/fusechat/pnagent-path" ( if path == "" then Nothing else Just path )
  }

mkCredentials = Credentials

serverURL :: App (Maybe URLString)
serverURL = dbMaybeRead "/fusechat/url"

pnAgentPath :: App String
pnAgentPath = fromMaybe "/Citrix/PNAgent/config.xml" <$> dbMaybeRead "/fusechat/pnagent-path"

withServerURL :: (URLString -> App a) -> App a
withServerURL f = serverURL >>= go where
  go Nothing    = throwError errorNoServerURL
  go (Just url) = f url

withPNAgentURL :: (URLString -> App a) -> App a
withPNAgentURL f = do
  agent <- pnAgentPath
  withServerURL $ \server -> f (server ++ agent)

-- xd = "http://beta.xendesktop.eng.citrite.net"
-- pnAgentConfig = "/Citrix/PNAgent/config.xml"
--xd = "https://engxd.citrix.com"
--pnAgentConfig = "/Citrix/ipad/config.xml"

request :: Req a -> App a
request r = from =<< liftIO (doRequest r) where
  from (Left err) = throwError (errorCurl err)
  from (Right v ) = return v

listDesktops :: Credentials -> App [Application]
listDesktops credentials = do
  cfg  <- withPNAgentURL pnAgentConfig
  apps <- request (appRequest cfg credentials)
  return (filter appIsDesktop apps)

getLaunchRef :: Credentials -> Resource -> App Launcher
getLaunchRef credentials app = do
  cfg <- withPNAgentURL pnAgentConfig
  request (launcherRequest cfg credentials app)

-- gets PNAgent config either from cache or downloads it & caches
pnAgentConfig :: URLString -> App PNConfig
pnAgentConfig url = from =<< lookupCachedPNConfig url where
    from (Just c) = return c
    from Nothing  = do
      c <- request (pnConfigRequest url)
      cachePNConfig url c
      return c

exportApp :: Application -> Map String String
exportApp app
  = Map.fromList
  [ ("inname", strResource $ appInName app)
  , ("fname", appFName app)
  , ("description", appDescription app) ]

exportLauncher :: Launcher -> String
exportLauncher = strLauncher
