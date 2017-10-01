from __future__ import division, print_function
import numpy as np
import os
import sys
import time
import lmdb
import shutil
import traceback
import math
from caffe2.python import core, model_helper, net_drawer, workspace, visualize, brew
import caffe2.python.predictor.predictor_exporter as pe
import caffe2.python.predictor.predictor_py_utils as pred_utils
from caffe2.python.predictor_constants import predictor_constants as pc
from caffe2.proto import caffe2_pb2

BATCH_SIZE=16
LEARN_RATE=-0.01
train_iters = 20000

#root_folder = "/home/jwatte/trainingdata/donkey_racing/network"
root_folder = "/home/jwatte/trainingdata"
#data_folder = "/home/jwatte/trainingdata/2017-09-07-8am-park"
data_folder = os.path.join(root_folder, 'temp_oakland')

def AddInput(model, batch_size, db, db_type):
    data_uint8, label = model.TensorProtosDBInput(
        [], ["data_uint8", "label"], batch_size=batch_size,
        db=db, db_type=db_type)
    data = model.Cast(data_uint8, "data_cast", to=core.DataType.FLOAT)
    data = model.Scale(data, data, scale=float(1./255))
    data = model.StopGradient(data, data)
    return data, label

def AddNetModel(model, data):
    # 182x70 -> 180x68
    conv1 = brew.conv(model, data, 'conv1', dim_in=1, dim_out=32, kernel=3)
    # 180x68 -> 90x34
    pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
    relu1 = brew.relu(model, pool1, 'relu1')
    # 90x34 -> 88x32
    conv2 = brew.conv(model, relu1, 'conv2', dim_in=32, dim_out=48, kernel=3)
    # 88x32 -> 44x16
    pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
    relu2 = brew.relu(model, pool2, 'relu2')
    # 44x16 -> 40x12
    conv3 = brew.conv(model, relu2, 'conv3', dim_in=48, dim_out=64, kernel=5)
    # 40x12 -> 20x6
    pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
    relu3 = brew.relu(model, pool3, 'relu3')
    # 20x6 -> 18x4
    conv4 = brew.conv(model, relu3, 'conv4', dim_in=64, dim_out=80, kernel=3)
    # 18x4 -> 9x2
    pool4 = brew.max_pool(model, conv4, 'pool4', kernel=2, stride=2)
    relu4 = brew.relu(model, pool4, 'relu4')
    # 256 FC
    fc5 = brew.fc(model, relu4, 'fc5', dim_in=9*2*80, dim_out=256)
    relu5 = brew.relu(model, fc5, 'relu5')
    fc6 = brew.fc(model, relu5, 'fc6', dim_in=256, dim_out=64)
    relu6 = brew.relu(model, fc6, 'relu6')
    output = brew.fc(model, relu6, 'output', dim_in=64, dim_out=2)
    return output

def AddTrainingOperators(model, output, label):
    #xent = model.LabelCrossEntropy([output, label], 'xent')
    xdiff = model.Sub([output, label], "xdiff")
    diffsq = model.DotProduct([xdiff, xdiff], "diffsq")
    #loss = model.Abs([xdiff], "loss")
    loss = model.AveragedLoss(diffsq, "loss")
    model.AddGradientOperators([loss])
    ITER = brew.iter(model, "iter")
    stepsize = int(train_iters / 30000)
    if stepsize < 1:
        stepsize = 1
    if stepsize > 25:
        stepsize = 25
    assert(LEARN_RATE < 0)
    LR = model.LearningRate(
        ITER, "LR", base_lr=LEARN_RATE, policy="step", stepsize=1, gamma=0.99999 )
    ONE = model.param_init_net.ConstantFill([], "ONE", shape=[1], value=1.0)
    for param in model.params:
        param_grad = model.param_to_grad[param]
        model.WeightedSum([param, ONE, param_grad, LR], param)
    model.Checkpoint([ITER] + model.params, [],
        db="checkpoint_%06d.lmdb", db_type="lmdb", every=10000)

def AddBookkeepingOperators(model):
    model.Print('loss', [], to_file=1)
    for param in model.params:
        model.Summarize(param, [], to_file=1)
    model.Summarize(model.param_to_grad[param], [], to_file=1)

def build_networks():
    arg_scope = {"order": "NCHW", "use_cudnn":True}
    train_model = model_helper.ModelHelper(name="donkey_train", arg_scope=arg_scope)
    dbpath = os.path.join(data_folder, 'train')
    print('dbpath = %s' % (dbpath,))
    data, label = AddInput(
        train_model, batch_size=BATCH_SIZE,
        db=dbpath,
        db_type='lmdb')
    output = AddNetModel(train_model, data)
    AddTrainingOperators(train_model, output, label)
    AddBookkeepingOperators(train_model)

    test_model = model_helper.ModelHelper(
        name="donkey_test", arg_scope=arg_scope, init_params=False)
    data, label = AddInput(
        test_model, batch_size=1,
        db=os.path.join(data_folder, 'test'),
        db_type='lmdb')
    output = AddNetModel(test_model, data)

    deploy_model = model_helper.ModelHelper(
        name="donkey_deploy", arg_scope=arg_scope, init_params=False)
    AddNetModel(deploy_model, "input")

    return (train_model, test_model, deploy_model)

def write_block(fil, nam, info, blob, bsize):
    fil.write("%s\n" % (nam,))
    fil.write("%s\n" % (info,))
    fil.write("%d\n" % (bsize,))
    fil.write(blob)
    fil.write("\n-\n")

def save_trained_model(deploy_model):
    savepath = os.path.join(root_folder, "donkey_model_protos.crunk")
    print('savepath = %s' % (savepath,))
    try:
        shutil.rmtree(savepath)
    except:
        pass
    with open(savepath, 'wb') as of:
        of.write("crunk 1.0\n")
        ss = deploy_model.net.Proto().SerializeToString()
        write_block(of, ':NET', '', ss, len(ss))
        parameters = [str(b) for b in deploy_model.params]
        for i in parameters:
            b = workspace.FetchBlob(i)
            print(i + ": " + repr(b.shape))
            write_block(of, i, repr(b.shape), b, b.size * b.itemsize)

    print("The deploy model is saved to: " + savepath)


def load_crunk(name, device_opts=None):
    with open(name, 'rb') as rdf:
        l = rdf.readline()
        if l <> 'crunk 1.0\n':
            raise RuntimeError(name + ': not a crunk 1.0 file')
        while True:
            name = rdf.readline()
            if name == '':
                break
            name = name.strip()
            info = rdf.readline()
            info = info.strip()
            shape = None
            if info <> '':
                shape = eval(info.strip())
            size = rdf.readline()
            size = int(size.strip())
            block = rdf.read(size)
            if shape:
                print("# ", name, shape, size, len(block))
                block = np.fromstring(block, dtype=np.float32)
                block = np.reshape(block, shape)
                workspace.FeedBlob(name, block, device_opts)
            empty = rdf.readline()
            dash = rdf.readline()
            if dash.strip() <> '-':
                raise RuntimeError(name + ': mis-formatted crunk 1.0 file (got ' + dash + ' expected "-")')

