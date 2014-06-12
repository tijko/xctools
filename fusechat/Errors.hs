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

{-# LANGUAGE FlexibleContexts #-}
module Errors (
               failIO
             , FuseChatError
             , errorNoServerURL
             , errorCurl
             , throwError, catchError
             ) where

import Control.Exception
import Control.Monad
import Control.Monad.Error

import Rpc.Core
import Curl

data FuseChatError = RError RpcCall RemoteErr -- remote
                   | DError ErrorCode String  -- local

type ErrorCode = Int

instance IsRemoteError FuseChatError where
    fromRemoteErr = RError
    toRemoteErr (RError _ dbusE) = Just dbusE
    toRemoteErr _                = Nothing

instance Show FuseChatError where
    show (DError code s)        = show code ++ ":" ++ s
    show (RError call dbus_err) = show dbus_err

-- List of errors thrown to clients
failIO :: (MonadError FuseChatError m) => String -> m a
failIO msg = throwError $ DError 600 ("IO error: " ++ msg)

errorNoServerURL  = DError 601 "XenDesktop server URL is not configured"
errorCurl (CurlError code1 code2) = DError 602 $ "CURL error " ++ show code1 ++ " " ++ show code2
