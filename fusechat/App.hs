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

{-# LANGUAGE GeneralizedNewtypeDeriving, MultiParamTypeClasses, FlexibleInstances #-}
module App
       ( App
       , runApp
       , appContext
       , newAppContext
       , lookupCachedPNConfig
       , cachePNConfig
       ) where

import Data.Map (Map)
import qualified Data.Map as Map
import Control.Applicative
import Control.Concurrent
import Control.Monad.Error
import Control.Monad.Trans
import Control.Monad.Reader
import qualified Control.Exception as E
import Rpc.Core
import Tools.FreezeIOM

import Errors
import Rpc
import Protocol

data AppContext
   = AppContext {
       appPNConfigs :: MVar (Map URLString PNConfig)
     }

newtype App a
      = App { unApp :: ReaderT AppContext Rpc a }
    deriving (Functor, Applicative, Monad, MonadError FuseChatError)

runApp :: AppContext -> App a -> Rpc a
runApp context app = runReaderT (unApp app) context

instance MonadIO App where
  liftIO io = liftRpc (liftIO io)

instance MonadRpc FuseChatError App where
  rpcGetContext            = liftRpc rpcGetContext
  rpcLocalContext ctxf f   = appContext >>= \c -> liftRpc (rpcLocalContext ctxf (runApp c f))

appContext = App ask

liftRpc = App . lift

instance FreezeIOM FrozenApp (Either FuseChatError) App where
  freeze f = liftIO . f =<< ( FrozenApp <$> rpcGetContext <*> appContext )
  thaw (FrozenApp rpc_ctx app_ctx) f = rpc rpc_ctx (runApp app_ctx f)
  cont (Left err) = throwError err
  cont (Right v ) = return v

data FrozenApp = FrozenApp RpcContext AppContext

newAppContext :: IO AppContext
newAppContext =
    AppContext <$> newMVar Map.empty

lookupCachedPNConfig :: URLString -> App (Maybe PNConfig)
lookupCachedPNConfig url =
  do m <- (appPNConfigs <$> appContext) >>= liftIO . readMVar
     return (Map.lookup url m)

cachePNConfig :: URLString -> PNConfig -> App ()
cachePNConfig url cfg =
  do m <- appPNConfigs <$> appContext
     liftIO . modifyMVar_ m $ return . Map.insert url cfg
